// Module with all things related to serialising and deserialising data.
// It serialises data types according with the robots protocol.
// In this module only basic serialisation is handled but it is extensible
// to be used with other complex structures. It relies heavily on overloading
// the >> and << operators so if you provide your overloaded versions of those
// for different types then this can work.

#ifndef _SERIALISE_H_
#define _SERIALISE_H_

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

#include <concepts>
#include <vector>
#include <map>
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
  // {x.eof()} -> std::convertible_to<bool>;
};

// todo: comparing sizes instead of same_as? perhaps someone gives us an int
// and that should be fine?
// Byte order.
template <typename T>
constexpr T hton(T num)
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
constexpr T ntoh(T num)
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

// Class for data serialisation.
class Serialiser {
  std::vector<uint8_t> out;
public:
  void clean()
  {
    out = {};
  }
  
  std::vector<uint8_t> to_bytes() const
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

  template <typename T>
  void ser(const std::vector<T>& seq)
  {
    uint32_t len = static_cast<uint32_t>(seq.size());
    ser(len);

    for (const T& item : seq)
      *this << item;
  }

  template <typename K, typename V>
  void ser(const std::map<K, V>& map)
  {
    uint32_t len = static_cast<uint32_t>(map.size());
    ser(len);

    for (const auto& [k, v] : map)
      *this << k << v;
  }

  template <typename T1, typename T2>
  void ser(const std::pair<T1, T2>& pair)
  {
    *this << pair.first << pair.second;
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
    std::vector<uint8_t> buff = r.read(sizeof(T));
    item = ntoh<T>(*(T*)(buff.data()));
  }
  
  void deser(std::string& str)
  {
    uint8_t len;
    deser(len);
    std::vector<uint8_t> bytes = r.read(len);
    str.assign(reinterpret_cast<char*>(bytes.data()), len);
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

#endif  // _SERIALISE_H_
