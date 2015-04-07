#ifndef __h264_dpb_hpp__f3777383_4d26_4213_a2c5_eabedfb388f6__
#define __h264_dpb_hpp__f3777383_4d26_4213_a2c5_eabedfb388f6__

#include "h264-syntax.hpp"
#include "h264-slice.hpp"

// default implementation of h264 decoded picture buffer

namespace h264 {

enum class ref_type { none, short_term, long_term };

enum class structure_type { frame, top, bot, pair };
inline bool has_top(structure_type s) { return s != structure_type::bot; }
inline bool has_bot(structure_type s) { return s != structure_type::top; }

template<typename Buffer>
struct frame {
  frame(unsigned frame_num, structure_type structure) : frame_num(frame_num), structure(structure) {}

  unsigned frame_num;
  unsigned long_term_frame_idx = -1u;

  structure_type structure;

  template<typename T>
  struct field_base {
    int poc = 0; 
    ref_type rt = ref_type::none;
  
    friend bool is_short_term_reference(field_base const& f) { return f.rt == ref_type::short_term; }
    friend bool is_long_term_reference(field_base const& f) { return f.rt == ref_type::long_term; }
    friend bool is_reference(field_base const& f) { return f.rt != ref_type::none; }

    friend void mark_as_short_term_reference(field_base& f) { f.rt = ref_type::short_term; }
    friend void mark_as_long_term_reference(field_base& f, unsigned long_term_frame_idx) {
      f.rt = ref_type::long_term;
      get_frame(static_cast<T&>(f)).long_term_frame_idx = long_term_frame_idx;
    }
    friend void mark_as_unused_for_reference(field_base& f) { f.rt = ref_type::none; }
    friend Buffer frame_buffer(field_base const& f) { return get_frame(f).buffer; }
  };

  struct top_field : field_base<top_field> {} top;
  struct bot_field : field_base<bot_field> {} bot;

private:
  Buffer buffer;
public:
  bool needed_for_output = true;

  friend frame const& get_frame(top_field const& top) { return *utils::container_of(&top, &frame::top); }
  friend frame const& get_frame(bot_field const& bot) { return *utils::container_of(&bot, &frame::bot); };
  friend frame& get_frame(top_field& top) { return *utils::container_of(&top, &frame::top); }
  friend frame& get_frame(bot_field& bot) { return *utils::container_of(&bot, &frame::bot); };

  friend top_field const& top(frame const& f) { return f.top; }
  friend bot_field const& bot(frame const& f) { return f.bot; }
  friend top_field& top(frame& f) { return f.top; }
  friend bot_field& bot(frame& f) { return f.bot; }
  
  friend picture_type pic_type(frame const& f) { return picture_type::frame; }
  friend picture_type pic_type(top_field const& f) { return picture_type::top; }
  friend picture_type pic_type(bot_field const& f) { return picture_type::bot; }

  friend unsigned FrameNum(frame const& f) { return f.frame_num; }
  friend void FrameNum(frame& f, unsigned frame_num) { f.frame_num = frame_num; }
  friend unsigned LongTermFrameIdx(frame const& f) { return f.long_term_frame_idx; }

  friend bool is_short_term_reference(frame const& f) { return f.top.rt == ref_type::short_term && f.bot.rt == ref_type::short_term; }
  friend bool is_long_term_reference(frame const& f) { return f.top.rt == ref_type::long_term && f.bot.rt == ref_type::long_term; }

  friend void mark_as_unused_for_reference(frame& f) { f.top.rt = f.bot.rt = ref_type::none; }
  friend void mark_as_short_term_reference(frame& f) { f.top.rt = f.bot.rt = ref_type::short_term; }
  friend void mark_as_long_term_reference(frame& f, unsigned long_term_frame_idx) {
    f.top.rt = f.bot.rt = ref_type::long_term;
    f.long_term_frame_idx = long_term_frame_idx;
  }

  friend void TopFieldOrderCnt(frame& f, int cnt) { f.top.poc = cnt; }
  friend void BotFieldOrderCnt(frame& f, int cnt) { f.bot.poc = cnt; }
  friend int TopFieldOrderCnt(frame const& f) { return f.top.poc; }
  friend int BotFieldOrderCnt(frame const& f) { return f.bot.poc; }
  friend int PicOrderCnt(frame const& f) { 
    switch(f.structure) {
    case structure_type::top: return TopFieldOrderCnt(f);
    case structure_type::bot: return BotFieldOrderCnt(f);
    default: return std::min(TopFieldOrderCnt(f), BotFieldOrderCnt(f));
    }
  }

  friend Buffer frame_buffer(frame const& f) { return f.buffer; }
  friend void frame_buffer(frame& f, Buffer buffer) { f.buffer = std::move(buffer); }

  // returns true if frame consists of fields (i.e a single field or complementary pair)
  friend bool field_flag(frame const& f) { return f.structure != structure_type::frame; }

  friend bool is_needed_for_output(frame const& f) { return f.needed_for_output; }
  friend void mark_as_not_needed_for_output(frame& f) { f.needed_for_output = false; }
};

template<typename Buffer>
class decoded_picture_buffer : public std::vector<frame<Buffer>> {
  //utils::optional<picture_reference<typename std::vector<frame<Buffer>>::iterator>> curr_pic;
  picture_type curr_pic_type = picture_type::top;

  bool is_new_frame(bool IdrPicFlag, picture_type pt, unsigned frame_num, bool has_mmco5) {
    return !current_picture() || IdrPicFlag || !(opposite(pt) == pic_type(*current_picture())) || !(frame_num == FrameNum(*current_picture())) || has_mmco5;
  }
public:
  using std::vector<frame<Buffer>>::vector;

  utils::optional<picture_reference<typename std::vector<frame<Buffer>>::const_iterator>> current_picture() const {
    if(this->empty()) return utils::nullopt; 
    return picture_reference<typename std::vector<frame<Buffer>>::const_iterator>{this->end()-1, curr_pic_type};
  }

  utils::optional<picture_reference<typename std::vector<frame<Buffer>>::iterator>> current_picture() {
    if(this->empty()) return utils::nullopt; 
    return picture_reference<typename std::vector<frame<Buffer>>::iterator>{this->end()-1, curr_pic_type};
  }

  void new_picture(bool IdrPicFlag, picture_type pt, unsigned frame_num, bool has_mmco5, poc_t poc) {
    if(is_new_frame(IdrPicFlag, pt, frame_num, has_mmco5)) {
      this->push_back(frame<Buffer>{frame_num, static_cast<structure_type>(pt)});
    }
    else
      this->back().structure = structure_type::pair;

    curr_pic_type = pt;

    assert(!is_short_term_reference(*current_picture()) && !is_long_term_reference(*current_picture()));

    if(has_top(pt)) TopFieldOrderCnt(*current_picture(), poc.top);
    if(has_bot(pt)) BotFieldOrderCnt(*current_picture(), poc.bot);
  }
};


}

#endif
