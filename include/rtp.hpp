#ifndef __rtp_a271e547_1eab_4fa6_b13b_4598570fe259_hpp__
#define __rtp_a271e547_1eab_4fa6_b13b_4598570fe259_hpp__

#include <cstdint>
#include <chrono>
#include <vector>

#include "mpeg.hpp"
#include "utils.hpp"

namespace rtp {

class udp_packet {
  static constexpr std::size_t storage_size = 1500-5*4-2*4; // ethernet mtu - ip header - udp header
  std::unique_ptr<std::uint8_t[]> data;
  std::size_t used = 0;
public:
  constexpr udp_packet() = default;
  udp_packet(udp_packet&&) = default;
  udp_packet& operator=(udp_packet&&) = default;

  udp_packet(udp_packet const& p) : udp_packet() {
    reserve();
    memcpy(begin(), p.begin(), p.used);
    used = p.used;
  }

  udp_packet& operator=(udp_packet const& p) {
    resize(p.size());
    memcpy(begin(), p.begin(), p.used);
    return *this;
  }

  std::uint8_t* begin() { return &data[0]; }
  std::uint8_t* end() { return begin() + used; }
  std::uint8_t* end_of_storage() { return begin() + storage_size; }

  std::uint8_t const* begin() const { return &data[0]; }
  std::uint8_t const* end() const { return begin() + used; }
  std::uint8_t const* end_of_storage() const { return begin() + storage_size; }

  bool empty() const noexcept { return begin() == end(); }

  std::size_t capacity() const noexcept { return data ? storage_size : 0; } 
  std::size_t size() const noexcept { return used; }
 
  void reserve() { 
    if(!data) 
      data = decltype(data)(new std::uint8_t[storage_size]); 
  }

  void resize(std::size_t n) {
    if(n > storage_size) throw std::out_of_range("udp_packet: can't set size bigger than 1472 bytes");
    if(n) reserve();
    used = n;
  }

  constexpr static std::size_t max_size() noexcept { return storage_size; }

  friend bool operator == (udp_packet const& a, udp_packet const& b) {
    return a.empty() == b.empty() || (a.used == b.used && std::equal(a.begin(), a.end(), b.begin()));
  }

  friend bool operator < (udp_packet const& a, udp_packet const& b) {
    if(b.empty()) return false;
    if(a.empty()) return true;

    return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
  }

  friend bool operator > (udp_packet const& a, udp_packet const& b) {
    return b < a;
  }

  friend bool operator <= (udp_packet const& a, udp_packet const& b) {
    return !(a > b);
  }

  friend bool operator >= (udp_packet const& a, udp_packet const& b) {
    return !(a < b);
  }
};

struct rtp_header {
  net32_t storage[3];

  unsigned  v()  const { return get( 0.2_bf, storage[0]); }
  bool      p()  const { return get( 2.1_bf, storage[0]); }
  bool      x()  const { return get( 3.1_bf, storage[0]); }
  unsigned  cc() const { return get( 4.4_bf, storage[0]); }
  bool      m()  const { return get( 8.1_bf, storage[0]); }
  unsigned  pt() const { return get( 9.7_bf, storage[0]); }
  unsigned  sequence_number() const { return get(16.16_bf, storage[0]); }

  std::uint32_t ssrc() const { return get(0.32_bf, storage[1]); }
  std::uint32_t timestamp() const { return get(0.32_bf, storage[2]); }
  std::uint32_t csrc(std::size_t n) const { return get(0.32_bf, storage[2+n]); }

  std::size_t header_size() const { return sizeof(*this) + cc()*sizeof(std::uint32_t); }
};


struct rtp_packet : udp_packet {
  rtp_packet() = default;
  rtp_packet(rtp_packet&&) = default;
  rtp_packet& operator=(rtp_packet&&) = default;
  rtp_packet(rtp_packet const&) = default;
  rtp_packet& operator=(rtp_packet const&) = default;

private:
  explicit rtp_packet(udp_packet&& p) : udp_packet(std::move(p)) {}
public:

  static rtp_packet parse(udp_packet&& p) {
    if(p.size() < sizeof(rtp_header)) return rtp_packet();
    
    auto hdr = *reinterpret_cast<rtp_header*>(p.begin());

    if(hdr.v() != 2) return rtp_packet();
    if(p.size() < hdr.header_size()) return rtp_packet(); 
    if(hdr.p() && *(p.end()-1) > p.size() - hdr.header_size()) return rtp_packet();

    return rtp_packet(std::move(p));
  }

  rtp_header const& header() const { return *reinterpret_cast<const rtp_header*>(udp_packet::begin()); }
  
  unsigned  v()  const { return header().v(); }
  bool      p()  const { return header().p(); }
  bool      x()  const { return header().x(); }
  unsigned  cc() const { return header().cc(); }
  bool      m()  const { return header().m(); }
  unsigned  pt() const { return header().pt(); }
  unsigned  sequence_number() const { return header().sequence_number(); }
  
  std::uint32_t ssrc() const { return header().ssrc(); }
  std::uint32_t timestamp() const { return header().timestamp(); }
  std::uint32_t csrc(std::size_t n) const { return header().csrc(n); }

  std::uint8_t const* begin() const { return udp_packet::begin() + header().header_size(); }
  std::uint8_t const* end() const { return header().p() ? udp_packet::end() - *(udp_packet::end()-1) : udp_packet::end(); } 
};


inline constexpr bool sequence_number_compare(std::uint16_t a, std::uint16_t b) {
    //return (a < 0xFF00) ? ((b < 0xFF00) ? a < b : a > 0x100) : ((b > 0x100) ? a < b : true);
  return std::int16_t(b - a) > 0;
}

// rfc2250
struct mpeg_video_header {
  net32_t value;

  unsigned  mbz() const { return get( 0.5_bf, value);  }
  bool      t()   const { return get( 5.1_bf, value); }
  unsigned  tr()  const { return get( 6.10_bf, value); }
  bool      an()  const { return get(16.1_bf, value); }
  bool      n()   const { return get(17.1_bf, value); }
  bool      s()   const { return get(18.1_bf, value); }
  bool      b()   const { return get(19.1_bf, value); }
  bool      e()   const { return get(20.1_bf, value); }
  unsigned  p()   const { return get(21.3_bf, value); }
  bool      fbv() const { return get(24.1_bf, value); }
  unsigned  bfc() const { return get(25.3_bf, value); }
  bool      ffv() const { return get(28.1_bf, value); }
  unsigned  ffc() const { return get(29.3_bf, value); }

  bool extension_header_present() const { return t(); }
  unsigned temporal_reference() const { return tr(); }
  mpeg::picture_coding picture_coding_type() const { return mpeg::picture_coding(p()); }
  bool new_picture_header() const { return an() & n(); }
  bool sequence_header_present() const { return s(); }
  bool beginning_of_slice() const { return s(); }
  bool end_of_slise() const { return e(); }
  bool full_pel_backward_vector() const { return fbv(); }
  unsigned backward_f_code() const { return bfc(); }
  unsigned full_pel_forward_vector() const { return ffv(); }
  unsigned forward_f_code() const { return ffc(); }
};

struct mpeg2_extension_video_header {
  net32_t value;

  bool      x()     const { return get( 0.1_bf, value); }
  bool      e()     const { return get( 1.1_bf, value); }
  unsigned  f_00()  const { return get( 2.4_bf, value); }
  unsigned  f_01()  const { return get( 6.4_bf, value); }
  unsigned  f_10()  const { return get(10.4_bf, value); }
  unsigned  f_11()  const { return get(14.8_bf, value); }
  unsigned  dc()    const { return get(18.2_bf, value); }
  unsigned  ps()    const { return get(20.2_bf, value); }
  bool      t()     const { return get(22.1_bf, value); }
  bool      p()     const { return get(23.1_bf, value); }
  bool      c()     const { return get(24.1_bf, value); }
  bool      q()     const { return get(25.1_bf, value); }
  bool      v()     const { return get(26.1_bf, value); }
  bool      a()     const { return get(27.1_bf, value); }
  bool      r()     const { return get(28.1_bf, value); }
  bool      h()     const { return get(29.1_bf, value); }
  bool      g()     const { return get(30.1_bf, value); }
  bool      d()     const { return get(31.1_bf, value); } 
};

using video_timestamp_t = std::chrono::duration<std::uint32_t, std::ratio<1, 90000>>;

struct m2v_packet : rtp_packet {
  m2v_packet() = default;
  
  m2v_packet(m2v_packet&&) = default;
  m2v_packet& operator=(m2v_packet&&) = default;
  
  m2v_packet(m2v_packet const&) = default;
  m2v_packet& operator=(m2v_packet const&) = default;

  static m2v_packet parse(rtp_packet&& rtp) {
    if(rtp.size() < sizeof(mpeg_video_header)) return m2v_packet();

    auto& h = *reinterpret_cast<mpeg_video_header const*>(rtp.begin());
    if(h.mbz() != 0 || (h.p() < 1 || h.p() > 3) ||
       (h.t() && rtp.size() < sizeof(mpeg2_extension_video_header) + sizeof(mpeg_video_header)))
      return m2v_packet();

    m2v_packet r;
    static_cast<rtp_packet&>(r) = std::move(rtp);
    return std::move(r);
  }

  mpeg_video_header video_header() const { return *reinterpret_cast<mpeg_video_header const*>(rtp_packet::begin()); }
  bool video_extension_header_present() const { return video_header().t(); }
  mpeg2_extension_video_header video_extension_header() const;

  std::uint8_t const* begin() const {
    return rtp_packet::begin() + sizeof(mpeg_video_header) + (video_header().t() ? sizeof(mpeg2_extension_video_header) : 0);
  }

  std::uint8_t const* end() const { return rtp_packet::end(); }

  std::chrono::duration<std::uint32_t, std::ratio<1,90000>> timestamp() const {
    return std::chrono::duration<std::uint32_t, std::ratio<1,90000>>(rtp_packet::timestamp());
  }

  std::uint16_t temporal_reference() const { return video_header().tr(); }
  mpeg::picture_coding picture_coding_type() const { return video_header().picture_coding_type(); }
  bool has_beginning_of_slice() const { return video_header().b(); }
  bool has_end_of_slice() const { return video_header().e(); }
}; 

std::ostream& operator << (std::ostream& os, m2v_packet const& p) {
  return os << "m2v_packet{tr:" << p.temporal_reference() << ", seq:" << p.sequence_number() << ", t:" << p.video_extension_header_present() << 
    ", an:" << p.video_header().an() << ", n:" << p.video_header().n() << 
    ", s:" << p.video_header().s() << ", b:" << p.video_header().b() << ", e:" << p.video_header().e() 
    << "}";
}

struct m2v_au_assembler {
  struct access_unit {
    std::vector<m2v_packet> packets;
  
    std::uint16_t temporal_reference() const { return packets.front().temporal_reference(); }

    std::uint16_t min_seq() const { return packets.front().sequence_number(); }
    std::uint16_t max_seq() const { return packets.back().sequence_number(); }
  
    void push(m2v_packet&& p) {
      auto pos = std::int32_t(p.sequence_number() - min_seq());
      if(pos < 0) {
        packets.insert(packets.begin(), -pos, m2v_packet());
        packets.front() = std::move(p);
      }
      else if(pos >= packets.size()) {
        packets.resize(pos+1);
        packets[pos] = std::move(p);
      }
      else {
        packets[pos] = std::move(p);
      }
    }
  };

  std::vector<access_unit> units;

  void operator()(m2v_packet p) {
    auto i = std::lower_bound(units.begin(), units.end(), p.sequence_number(),
      utils::make_compare_by_key([](access_unit const& a) { return a.min_seq(); }, sequence_number_compare));

    if(i != units.begin() && (i-1)->temporal_reference() == p.temporal_reference())
      --i;

    if(i == units.end() || i->temporal_reference() != p.temporal_reference()) {
      std::cout << "new_access_unit:" << p << std::endl; 
      i = units.insert(i, {{std::move(p)}});
    }
    else {
      i->push(std::move(p));
    }
  }
};

struct h264_packet : rtp_packet {};

struct h264_defragmenter {
  using nalu = std::vector<h264_packet>;
};


}

#endif
