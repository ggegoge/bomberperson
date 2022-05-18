// Marshalling and unmarshalling of data.

// It serialises data types according to simple protocol. Here only basic
// serialisation is handled but this serves as an extensible layer of
// abstraction (aka marshalling framework) that can still be used with other
// complex structures. It relies heavily on overloading the >> and << operators
// so if you provide your overloaded versions of those for different types then
// this can still work for them. You can see it done in messages.h/cc.

#ifndef _MARSHAL_H_
#define _MARSHAL_H_

// There are problems on macos with changing the byte order.
#ifdef __MACH__
#  include <libkern/OSByteOrder.h>
#  define htobe64(x) OSSwapHostToBigInt64(x)
#  define be64toh(x) OSSwapBigInt64ToHost(x)
#else
#  include <endian.h>
#endif // __MACH__

// htonl and htons
#include <arpa/inet.h>

#include <exception>
#include <stdexcept>
#include <concepts>
#include <vector>
#include <tuple>
#include <map>
#include <set>
#include <string>
#include <cstddef>
#include <cstdint>

// This concept describes a class from which the deserialiser will read bytes.
// It should be possible to extract a chosen number of those depending on what
// do you want to read.
template <typename T>
concept Readable = requires (T x, size_t nbytes)
{
  {x.read(nbytes)} -> std::same_as<std::vector<uint8_t>>;
};

// This concepts ensures that type Seq represents an iterable sized container.
// The constraints are not overly strong as I wanted to keep that simple.
template <typename Seq>
concept Iterable = requires (Seq seq)
{
  {seq.size()} -> std::integral;
  {seq.cbegin()};
  {seq.cend()};
};

// todo: comparing sizes instead of same_as? perhaps someone gives us an int
// and that should be fine?
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
  if constexpr (std::is_same_v<T, uint64_t>)
    return htobe64(num);
  else if constexpr (std::is_same_v<T, uint32_t>)
    return ntohl(num);
  else if constexpr (std::is_same_v<T, uint16_t>)
    return ntohs(num);
  else
    return num;
}

class UnmarshallingError : public std::runtime_error {
public:
  UnmarshallingError()
    : std::runtime_error("Error in deserialisaion according to robots protocol") {}
  UnmarshallingError(const std::string& msg) : std::runtime_error(msg) {}
};

// Class for data serialisation.
class Serialiser {
  std::vector<uint8_t> out;
public:
  void clean()
  {
    out = {};
  }

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

  // This combines the action of clean() and to_bytes() as it is often useful.
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
    *(T*)(bytes) = hton<T>(item);

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
    ser<uint8_t>(static_cast<uint8_t>(str.length()));

    for (char c : str)
      out.push_back(static_cast<uint8_t>(c));
  }

  template <Iterable Seq>
  void ser(const Seq& seq)
  {
    uint32_t len = static_cast<uint32_t>(seq.size());
    ser(len);

    for (const auto& item : seq)
      *this << item;
  }

  // Note: thank's to this function map is also serialisable due to being and
  // iterable of key-value pairs.
  template <typename T1, typename T2>
  void ser(const std::pair<T1, T2>& pair)
  {
    *this << pair.first << pair.second;
  }

  template <typename ... Ts>
  void ser(const std::tuple<Ts...>& tuple)
  {
    std::apply([this] (auto... v) { ( *this << ... << v); }, tuple);
  }

  template <typename T>
  Serialiser& operator<<(const T& item)
  {
    ser(item);
    return *this;
  }
};

template <Readable R>
class Deserialiser {
  R r;

public:
  Deserialiser(const R& r) : r(r) {}
  Deserialiser(R&& r) : r(std::move(r)) {}

  // This allows for changing and accessing the underlying readable.
  R& readable()
  {
    return r;
  }

  // Note: we do not offer a function for deserialising enums as it is quite
  // dangerous considering there is no range check done on them upon conversion.
  template <std::integral T>
  void deser(T& item) requires (!std::is_enum_v<T>)
  {
    try {
      std::vector<uint8_t> buff = r.read(sizeof(T));
      item = ntoh<T>(*(T*)(buff.data()));
    } catch (std::exception& e) {
      std::string err = "Failed to unmarshal a number: ";
      throw UnmarshallingError(err + e.what());
    }
  }
  
  void deser(std::string& str)
  {
    try {
      uint8_t len;
      deser(len);
      std::vector<uint8_t> bytes = r.read(len);
      str.assign(reinterpret_cast<char*>(bytes.data()), len);
    } catch (std::exception& e) {
      std::string err = "Failed to unmarshal a string: ";
      throw UnmarshallingError(err + e.what());
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
  
  template <typename T>
  Deserialiser& operator>>(T& item)
  {
    deser(item);
    return *this;
  }
};

#endif  // _MARSHAL_H_
