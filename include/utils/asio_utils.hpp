#ifndef __asio_ioctl_hpp__baf5c87b_c6a2_4781_b9e7_ee8537bdf323__
#define __asio_ioctl_hpp__baf5c87b_c6a2_4781_b9e7_ee8537bdf323__

#include <asio.hpp>

namespace utils {

template<typename Data>
struct generic_write_buffer {
  Data data;
};

template<typename Data>
Data const& data(generic_write_buffer<Data> const& buf) { return buf.data; }

template<typename Data>
Data const& data(generic_write_buffer<std::reference_wrapper<Data>> const& buf) { return buf.data.get(); }

template<typename Data>
int perform_write(int fd, generic_write_buffer<Data> const& buf) {
  return ::write(fd, &data(buf), sizeof(data(buf)));
}

template<typename Data>
struct generic_read_buffer {
  Data data;
};

template<typename Data>
Data const& data(generic_read_buffer<Data> const& buf) { return buf.data; }

template<typename Data>
int perform_read(int fd, generic_read_buffer<Data*> const& buf) { 
  return ::read(fd, data(buf), sizeof(*data(buf)));
}

template<int Code, typename Data>
struct ioctl_data {
  Data data;
};

template<int Code>
struct ioctl_data<Code, void> {};

template<int Code, typename Data>
Data const& data(ioctl_data<Code, Data> const& data) { return data.data; }

template<int Code, typename Data>
Data* data(ioctl_data<Code, std::reference_wrapper<Data>> const& data) { return &data.data.get(); }

template<int Code, typename Data>
int perform_ioctl(int fd, ioctl_data<Code, Data> const& d) { return ::ioctl(fd, Code, data(d)); }

template<int Code>
int perform_ioctl(int fd, ioctl_data<Code, void> const& d) { return ::ioctl(fd, Code); }

template<int Code, typename Data>
int perform_write(int fd, generic_write_buffer<ioctl_data<Code, Data>> const& buf) {
  return perform_ioctl(fd, data(buf));
}

template<int Code, typename Data>
int perform_read(int fd, generic_read_buffer<ioctl_data<Code, Data*>> const& buf) {
  return perform_ioctl(fd, data(buf));
}

template<int Code, typename Data>
auto make_ioctl_write_buffer(Data&& d) -> generic_write_buffer<ioctl_data<Code, typename std::decay<Data>::type>> { return {{std::forward<Data>(d)}}; }

template<int Code, typename Data>
auto make_ioctl_read_buffer(Data&& d) -> generic_read_buffer<ioctl_data<Code, std::decay_t<Data>>> { return {{std::forward<Data>(d)}}; }
}
 
namespace asio { namespace detail {

template<typename Data>
class descriptor_write_op_base<utils::generic_write_buffer<Data>> : public ::asio::detail::reactor_op {
public:
  descriptor_write_op_base(int descriptor, utils::generic_write_buffer<Data> const& buffer, func_type complete_func)
    : reactor_op(&descriptor_write_op_base::do_perform, complete_func),
      descriptor(descriptor),
      buffer(buffer)
  {}

  static bool do_perform(reactor_op* base) {
    descriptor_write_op_base* o(static_cast<descriptor_write_op_base*>(base));

    o->ec_ = asio::error_code();

    auto r = perform_write(o->descriptor, o->buffer);

    if(r < 0) o->ec_ = asio::error_code(errno, asio::error::get_system_category());

    if(!o->ec_)
      o->bytes_transferred_ = r;

    return o->ec_ != error::would_block;
  }
private:
  int descriptor;
  utils::generic_write_buffer<Data> buffer;
};

template<typename Data>
class descriptor_read_op_base<utils::generic_read_buffer<Data>> : public ::asio::detail::reactor_op {
public:
  descriptor_read_op_base(int descriptor, utils::generic_read_buffer<Data> const& buffer, func_type complete_func) :
    reactor_op(&descriptor_read_op_base::do_perform, complete_func),
    descriptor(descriptor),
    buffer(buffer)
  {}

  static bool do_perform(reactor_op* base) {
    descriptor_read_op_base* o(static_cast<descriptor_read_op_base*>(base));

    o->ec_ = asio::error_code();
   
    auto r = perform_read(o->descriptor, o->buffer); 
    if(r < 0) o->ec_ = asio::error_code(errno, asio::error::get_system_category());
 
    if(!o->ec_) o->bytes_transferred_ = r;
     
    return o->ec_ != error::would_block; 
  }
private:
  int descriptor;
  utils::generic_read_buffer<Data> buffer;
};
 
template<typename Data>
class buffer_sequence_adapter<const_buffer, utils::generic_write_buffer<Data>> {
public:
  static bool all_empty(utils::generic_write_buffer<Data> const&) { return false; }
};

template<typename Data>
class buffer_sequence_adapter<mutable_buffer, utils::generic_read_buffer<Data>> {
public:
  static bool all_empty(utils::generic_read_buffer<Data> const&) { return false; }
};


}}

#endif
