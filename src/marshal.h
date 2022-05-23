// Marshalling and unmarshalling of data aka (de)serialisation.
// These two terms are used quite freely here.

// It serialises data types according to simple protocol. Here only basic
// serialisation is handled but this serves as an extensible layer of
// abstraction (aka marshalling framework) that can still be used with other
// complex structures: just write other overloads for stream operators that
// would be struct specific (but perhaps stick to tuples if you can).

#ifndef _MARSHAL_H_
#define _MARSHAL_H_

// Byte order
#include <arpa/inet.h>
#include <endian.h>

#include <type_traits>
#include <stdexcept>
#include <concepts>
#include <variant>
#include <ranges>
#include <string>
#include <vector>
#include <tuple>
#include <map>
#include <set>
#include <cstddef>
#include <cstdint>

// This concept describes a class from which the deserialiser can read bytes.
// It should be possible to extract a chosen number of those depending on what
// do you want to read and it should tell you how many bytes are there to be
// read at any given time.
template <typename T>
concept Readable = requires (T x, size_t nbytes) {
  {x.read(nbytes)} -> std::same_as<std::vector<uint8_t>>;
  {x.avalaible()} -> std::same_as<size_t>;
};

// Changing the byte order. Numbers are serialised in the network order.
template <typename T>
inline constexpr T hton(T num)
{
  if constexpr (std::same_as<T, uint64_t>)
    return be64toh(num);
  else if constexpr (std::same_as<T, uint32_t>)
    return htonl(num);
  else if constexpr (std::same_as<T, uint16_t>)
    return htons(num);
  else
    return num;
}

template <typename T>
inline constexpr T ntoh(T num)
{
  if constexpr (std::same_as<T, uint64_t>)
    return htobe64(num);
  else if constexpr (std::same_as<T, uint32_t>)
    return ntohl(num);
  else if constexpr (std::same_as<T, uint16_t>)
    return ntohs(num);
  else
    return num;
}

// Unmarshalling may fail whereas marshalling in our protocol is infalliable.
class UnmarshallingError : public std::runtime_error {
public:
  UnmarshallingError() : runtime_error{"Error in unmarshalling!"} {}
  UnmarshallingError(const std::string& msg) : runtime_error{msg} {}
};

class Serialiser {
  std::vector<uint8_t> out;
public:
  size_t size() const
  {
    return out.size();
  }

  std::vector<uint8_t> to_bytes() const
  {
    return out;
  }

  std::vector<uint8_t>& to_bytes()
  {
    return out;
  }

  // Get current output and clean it.
  std::vector<uint8_t> drain_bytes()
  {
    std::vector<uint8_t> empty;
    std::swap(empty, out);
    return empty;
  }

  // The integral constraint is here so that this function does not get called
  // on non-integral types thus it obliges the programmer to provide their
  // overloads for more complex types and structures.
  template <std::integral T>
  void ser(const T& item) requires (!std::is_enum_v<T>)
  {
    uint8_t bytes[sizeof(T)];
    *reinterpret_cast<T*>(bytes) = hton<T>(item);

    for (uint8_t byte : bytes)
      out.push_back(byte);
  }

  // All enums are serialised as one-byte integers!
  template <typename T>
  void ser(const T& enum_item) requires std::is_enum_v<T>
  {
    ser(static_cast<uint8_t>(enum_item));
  }
  
  void ser(const std::string& str)
  {
    ser(static_cast<uint8_t>(str.length()));

    for (char c : str)
      out.push_back(static_cast<uint8_t>(c));
  }

  // Marshalling of sets, vectors, maps etc.
  template <std::ranges::sized_range Seq>
  void ser(const Seq& seq)
  {
    ser(static_cast<uint32_t>(std::ranges::size(seq)));

    for (const auto& item : seq)
      *this << item;
  }

  // Note: thanks to this function std::map is also serialisable due to being an
  // iterable of key-value pairs.
  template <typename T1, typename T2>
  void ser(const std::pair<T1, T2>& pair)
  {
    *this << pair.first << pair.second;
  }

  // This is why it is easier to use a tuple than a struct.
  template <typename... Ts>
  void ser(const std::tuple<Ts...>& tuple)
  {
    std::apply([this] <typename... T> (const T&... v) {
        (*this << ... << v);
      }, tuple);
  }

  template <typename... Ts>
  void ser(const std::variant<Ts...>& var)
  {
    ser(static_cast<uint8_t>(var.index()));
    std::visit([this] <typename T> (const T& x) { *this << x; }, var);
  }

  // Empty structures are useful for variants hence we allow those.
  template <typename T>
  void ser(const T&) requires std::is_empty_v<T> {}

  // The serialisation operator proper.
  template <typename T>
  Serialiser& operator<<(const T& item)
  {
    ser(item);
    return *this;
  }
};

// Data deserialisation is just serialisation but conversly.
template <Readable R>
class Deserialiser {
  R r;

public:
  Deserialiser() : r{} {}
  Deserialiser(const R& r) : r{r} {}
  Deserialiser(R&& r) : r{std::move(r)} {}

  // This allows for changing and accessing the underlying readable.
  R& readable()
  {
    return r;
  }

  size_t avalaible() const
  {
    return r.avalaible();
  }

  // Data not ending can be sometimes considered an unmarshalling error.
  void no_trailing_bytes() const
  {
    if (avalaible())
      throw UnmarshallingError{"Trailing bytes!"};
  }

  // Note: we do not offer a function for deserialising enums as it is quite
  // dangerous considering there is no range check done on them upon conversion.
  template <std::integral T>
  void deser(T& item) requires (!std::is_enum_v<T>)
  {
    try {
      std::vector<uint8_t> buff = r.read(sizeof(T));
      item = ntoh<T>(*reinterpret_cast<T*>(buff.data()));
    } catch (std::exception& e) {
      std::string err = "Failed to unmarshal a number: ";
      throw UnmarshallingError{err + e.what()};
    }
  }

  void deser(std::string& str)
  {
    try {
      uint8_t len;
      deser(len);
      std::vector<uint8_t> bytes = r.read(len);
      str.assign(reinterpret_cast<char*>(bytes.data()), len);
    } catch (UnmarshallingError& e) {
      throw;
    } catch (std::exception& e) {
      std::string err = "Failed to unmarshal a string: ";
      throw UnmarshallingError{err + e.what()};
    }
  }

  template <typename T>
  void deser(std::vector<T>& seq)
  {
    uint32_t len;
    deser(len);

    for (uint32_t i = 0; i < len; ++i) {
      T x;
      *this >> x;
      seq.push_back(x);
    }
  }

  template <typename T>
  void deser(std::set<T>& set)
  {
    uint32_t len;
    deser(len);

    for (uint32_t i = 0; i < len; ++i) {
      T x;
      *this >> x;
      set.insert(x);
    }
  }

  template <typename K, typename V>
  void deser(std::map<K, V>& map)
  {
    uint32_t len;
    deser(len);

    for (uint32_t i = 0; i < len; ++i) {
      K k;
      V v;
      *this >> k >> v;
      map.insert({k, v});
    }
  }

  template <typename T1, typename T2>
  void deser(std::pair<T1, T2>& pair)
  {
    *this >> pair.first >> pair.second;
  }

  template <typename... Ts>
  void deser(std::tuple<Ts...>& tuple)
  {
    std::apply([this] <typename... T> (T&... v) {
        (*this >> ... >> v);
      }, tuple);
  }

  // The trickiest. First create a default variant based on the index and then
  // fill the stored value.
  template <typename... Ts>
  void deser(std::variant<Ts...>& var)
  {
    using Var = std::variant<Ts...>;
    uint8_t kind;
    deser(kind);
    var = variant_from_index<Var>(kind);
    std::visit([this] <typename T> (T& x) { *this >> x; }, var);
  }

  template <typename T>
  void deser(T&) requires std::is_empty_v<T> {}

  template <typename T>
  Deserialiser& operator>>(T& item)
  {
    deser(item);
    return *this;
  }

private:
  // Helper function for creating a variant from chosen index.
  // source: https://stackoverflow.com/a/60567091/9058764
  template <class Var, size_t I = 0>
  Var variant_from_index(size_t index)
  {
    if constexpr (I >= std::variant_size_v<Var>) {
      throw UnmarshallingError{"Index does not match the variant!"};
    } else {
      // This changes runtime index into a compile time index, brilliant.
      return index == 0
        ? Var{std::in_place_index<I>}
        : variant_from_index<Var, I + 1>(index - 1);
    }
  }
};

#endif  // _MARSHAL_H_
