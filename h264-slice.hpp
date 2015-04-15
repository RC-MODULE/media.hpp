#ifndef __h264_slice_hpp_b2a94c43_5719_4c40_b8cf_b6a430219287__
#define __h264_slice_hpp_b2a94c43_5719_4c40_b8cf_b6a430219287__

// h264-slice.hpp - implements slice header decoding processes 
// defined in section 8.2 "Slice Decoding process" of H.264

#include <numeric>
#include "h264-syntax.hpp"

namespace media {
namespace h264 {

// 7.4.1.2.4 Detection of the first VCL NAL unit of a primary coded picture
struct new_picture_detector {
  slice_identity_header h;

  bool operator()(slice_header const& slice) {
    bool r = are_different_pictures(h, slice);
    h = slice; 

    return r;
  } 
};

struct poc_t {
  int top;
  int bot;
};

// 8.2.1 Decoding process for picture order count
struct poc_decoder {
  unsigned  log2_max_frame_num_minus4; 
  unsigned  pic_order_cnt_type;
  // if( pic_order_cnt_type == 0 )
  unsigned log2_max_pic_order_cnt_lsb_minus4;
  // else if( pic_order_cnt_type == 1 )
  int       offset_for_non_ref_pic;
  int       offset_for_top_to_bottom_field;
  std::vector<int> offset_for_ref_frame;

  poc_decoder() {}
  poc_decoder(seq_parameter_set const& sps) :
    log2_max_frame_num_minus4(sps.log2_max_frame_num_minus4),
    pic_order_cnt_type(sps.pic_order_cnt_type),
    log2_max_pic_order_cnt_lsb_minus4(sps.log2_max_pic_order_cnt_lsb_minus4),
    offset_for_non_ref_pic(sps.offset_for_non_ref_pic),
    offset_for_top_to_bottom_field(sps.offset_for_top_to_bottom_field),
    offset_for_ref_frame(sps.offset_for_ref_frame),
    prevPicOrderCntMsb(0),
    prevPicOrderCntLsb(0)
  {}

  unsigned MaxFrameNum() const { return 1 << (log2_max_frame_num_minus4 + 4); } 
  unsigned MaxPicOrderCntLsb() const { return 1 << (log2_max_pic_order_cnt_lsb_minus4 + 4); }

  union {
    struct {
      unsigned prevPicOrderCntMsb;
      unsigned prevPicOrderCntLsb;
    }; 

    struct {
      unsigned prevFrameNum;
      unsigned prevFrameNumOffset;
    };
  };

  unsigned ExpectedDeltaPerPicOrderCntCycle() const {
    return std::accumulate(offset_for_ref_frame.begin(), offset_for_ref_frame.end(), 0);
  }

  poc_t poc_cnt_type0(bool IdrPicFlag, unsigned nal_ref_idc, picture_type pic_type, unsigned pic_order_cnt_lsb, unsigned delta_pic_order_cnt_bottom, bool has_mmco5) {
    if(IdrPicFlag) {
      prevPicOrderCntLsb = 0;
      prevPicOrderCntMsb = 0;
    }

    std::int32_t PicOrderCntMsb = prevPicOrderCntMsb;
   
    if(pic_order_cnt_lsb < prevPicOrderCntLsb && ((prevPicOrderCntLsb - pic_order_cnt_lsb) >= (MaxPicOrderCntLsb()/2)))
      PicOrderCntMsb = prevPicOrderCntMsb + MaxPicOrderCntLsb();
    else if((pic_order_cnt_lsb > prevPicOrderCntLsb) && ((pic_order_cnt_lsb - prevPicOrderCntLsb) > (MaxPicOrderCntLsb()/2)))
      PicOrderCntMsb = prevPicOrderCntMsb - MaxPicOrderCntLsb();

    poc_t poc;
    if(pic_type != picture_type::bot)
      poc.top = PicOrderCntMsb + pic_order_cnt_lsb;
    
    if(pic_type == picture_type::frame)
      poc.bot = poc.top + delta_pic_order_cnt_bottom;
    else if(pic_type == picture_type::bot)
      poc.bot = PicOrderCntMsb + pic_order_cnt_lsb;

    if(nal_ref_idc) {
      if(has_mmco5) {
        prevPicOrderCntMsb = 0;
        prevPicOrderCntLsb = (pic_type != picture_type::bot) ? poc.top : 0;
      }
      else {
        prevPicOrderCntMsb = PicOrderCntMsb;
        prevPicOrderCntLsb = pic_order_cnt_lsb;
      }
    }

    return poc;
  }

  poc_t poc_cnt_type1(bool IdrPicFlag, unsigned nal_ref_idc, picture_type pic_type, unsigned frame_num, const int delta_pic_order_cnt[], bool has_mmco5) {
    unsigned FrameNumOffset;

    if(IdrPicFlag)
      FrameNumOffset = 0;
    else if(prevFrameNum > frame_num)
      FrameNumOffset = prevFrameNumOffset + MaxFrameNum();
    else
      FrameNumOffset = prevFrameNumOffset;

    prevFrameNum = has_mmco5 ? 0 : frame_num;
    prevFrameNumOffset = has_mmco5 ? 0 : FrameNumOffset;

    auto num_ref_frames_in_pic_order_cnt_cycle = offset_for_ref_frame.size();
    unsigned absFrameNum = 0;
    if(num_ref_frames_in_pic_order_cnt_cycle != 0) absFrameNum = FrameNumOffset + frame_num;

    if(nal_ref_idc == 0 && absFrameNum > 0) absFrameNum -= 1;

    unsigned picOrderCntCycleCnt = 0;
    unsigned frameNumInPicOrderCntCycle = 0;

    if(absFrameNum > 0) {
      picOrderCntCycleCnt = (absFrameNum - 1) /  num_ref_frames_in_pic_order_cnt_cycle;
      frameNumInPicOrderCntCycle = (absFrameNum - 1) % num_ref_frames_in_pic_order_cnt_cycle;
    }

    int expectedPicOrderCnt = 0;
    if(absFrameNum > 0) {
      expectedPicOrderCnt = picOrderCntCycleCnt * ExpectedDeltaPerPicOrderCntCycle();
      for(size_t i = 0; i <= frameNumInPicOrderCntCycle; ++i)
        expectedPicOrderCnt += offset_for_ref_frame[i];
    }

    if(nal_ref_idc == 0) expectedPicOrderCnt += offset_for_non_ref_pic;
  
    if(pic_type == picture_type::frame) {
      int top = expectedPicOrderCnt + delta_pic_order_cnt[0];
      return {top, top + offset_for_top_to_bottom_field + delta_pic_order_cnt[1]}; 
    }
    else if(pic_type == picture_type::top) {
      return {expectedPicOrderCnt + delta_pic_order_cnt[0], 0};
    }
    else {
      return {0, expectedPicOrderCnt + offset_for_top_to_bottom_field + delta_pic_order_cnt[0]};
    }
  } 

  poc_t poc_cnt_type2(bool IdrPicFlag, unsigned nal_ref_idc, picture_type pic_type, unsigned frame_num, bool has_mmco5) {
    unsigned FrameNumOffset;

    if(IdrPicFlag)
      FrameNumOffset = 0;
    else if(prevFrameNum > frame_num)
      FrameNumOffset = prevFrameNumOffset + MaxFrameNum();
    else
      FrameNumOffset = prevFrameNumOffset;

    prevFrameNum = has_mmco5 ? 0 : frame_num;
    prevFrameNumOffset = has_mmco5 ? 0 : FrameNumOffset;

    int tempPicOrderCnt =  2 * (FrameNumOffset + frame_num);
    if(nal_ref_idc == 0) tempPicOrderCnt -= 1;

    if(pic_type == picture_type::frame)
      return {tempPicOrderCnt, tempPicOrderCnt};
    else if(pic_type == picture_type::top)
      return {tempPicOrderCnt, 0};
    return {0, tempPicOrderCnt};
  }

  poc_t operator()(slice_header const& slice) {
    auto has_mmco5 = std::any_of(slice.mmcos.begin(), slice.mmcos.end(), [&](memory_management_control_operation const& op) { return op.id == 5; });

    switch(pic_order_cnt_type) {
    case 0: return poc_cnt_type0(slice.IdrPicFlag, slice.nal_ref_idc, slice.pic_type, slice.pic_order_cnt_lsb, slice.delta_pic_order_cnt_bottom, has_mmco5);
    case 1: return poc_cnt_type1(slice.IdrPicFlag, slice.nal_ref_idc, slice.pic_type, slice.frame_num, slice.delta_pic_order_cnt, has_mmco5);
    case 2: return poc_cnt_type2(slice.IdrPicFlag, slice.nal_ref_idc, slice.pic_type, slice.frame_num, has_mmco5);
    }
    
    throw std::logic_error("unsupported pic_order_cnt_type");
  }
};

// 8.2.4.1 Decoding process for picture numbers
template<typename C, typename P>
int FrameNumWrap(unsigned max_frame_num, C const& curr_pic, P const& pic) {
  return FrameNum(pic) > FrameNum(curr_pic) ? FrameNum(pic) - max_frame_num : FrameNum(pic);
}

template<typename C, typename P>
int PicNum(unsigned max_frame_num, C const& curr_pic, P const& pic) {
  auto n = FrameNumWrap(max_frame_num, curr_pic, pic);
  return pic_type(curr_pic) == picture_type::frame ? n : (2*n + (pic_type(pic) == pic_type(curr_pic)));
}

template<typename C>
int PicNum(C const& curr_pic) { return PicNum(16, curr_pic, curr_pic); }

template<typename C, typename P>
unsigned LongTermPicNum(C const& curr_pic, P const& pic) {
  return pic_type(curr_pic) == picture_type::frame ? LongTermFrameIdx(pic) : (2 * LongTermFrameIdx(pic) + (pic_type(pic) == pic_type(curr_pic)));
}

template<typename C>
int MaxPicNum(unsigned max_frame_num, C const& curr_pic) { return max_frame_num * (1 + (pic_type(curr_pic) != picture_type::frame)); }

// some infrastructure for working with generic decoded picture buffers
template<typename I>
struct picture_reference {
  I frame;
  picture_type pt;

  friend bool operator == (picture_reference const& a, picture_reference const& b) {
    return a.pt == b.pt && a.frame == b.frame;
  }

  friend unsigned FrameNum(picture_reference const& p) { return FrameNum(*p.frame); }
  friend void FrameNum(picture_reference const& p, unsigned frame_num) { FrameNum(*p.frame, frame_num); }
  friend unsigned LongTermFrameIdx(picture_reference const& p) { return LongTermFrameIdx(*p.frame); }
  friend picture_type pic_type(picture_reference const& p) { return p.pt; }

  friend unsigned PicOrderCnt(picture_reference const& p) {
    if(pic_type(p) == picture_type::top) return TopFieldOrderCnt(*p.frame);
    if(pic_type(p) == picture_type::bot) return BotFieldOrderCnt(*p.frame);
    return std::min(TopFieldOrderCnt(*p.frame), BotFieldOrderCnt(*p.frame));
  }

  friend unsigned TopFieldOrderCnt(picture_reference const& p) { return TopFieldOrderCnt(*p.frame); }
  friend void TopFieldOrderCnt(picture_reference const& p, unsigned cnt) { TopFieldOrderCnt(*p.frame, cnt); }
  
  friend unsigned BotFieldOrderCnt(picture_reference const& p) { return BotFieldOrderCnt(*p.frame); }
  friend void BotFieldOrderCnt(picture_reference const& p, unsigned cnt) { BotFieldOrderCnt(*p.frame, cnt); }
 
  friend bool is_short_term_reference(picture_reference const& p) {
    return (pic_type(p) == picture_type::frame && is_short_term_reference(*p.frame))
        || (pic_type(p) == picture_type::top && is_short_term_reference(top(*p.frame)))
        || (pic_type(p) == picture_type::bot && is_short_term_reference(bot(*p.frame))); 
  }

  friend bool is_long_term_reference(picture_reference const& p) {
    return (pic_type(p) == picture_type::frame && is_long_term_reference(*p.frame))
        || (pic_type(p) == picture_type::top && is_long_term_reference(top(*p.frame)))
        || (pic_type(p) == picture_type::bot && is_long_term_reference(bot(*p.frame)));
  }

  friend void mark_as_short_term_reference(picture_reference const& p) {
    if(pic_type(p) == picture_type::top)
      mark_as_short_term_reference(top(*p.frame));
    else if(pic_type(p) == picture_type::bot)
      mark_as_short_term_reference(bot(*p.frame));
    else
      mark_as_short_term_reference(*p.frame);
  }

  friend void mark_as_long_term_reference(picture_reference const& p, unsigned long_term_frame_idx) {
    if(pic_type(p) == picture_type::top)
      mark_as_long_term_reference(top(*p.frame), long_term_frame_idx);
    else if(pic_type(p) == picture_type::bot)
      mark_as_long_term_reference(bot(*p.frame), long_term_frame_idx);
    else
      mark_as_long_term_reference(*p.frame, long_term_frame_idx);
  }

  friend void mark_as_unused_for_reference(picture_reference const& p) {
    if(pic_type(p) == picture_type::top)
      mark_as_unused_for_reference(top(*p.frame));
    else if(pic_type(p) == picture_type::bot)
      mark_as_unused_for_reference(bot(*p.frame));
    else
      mark_as_unused_for_reference(*p.frame);
  }
};

template<typename I>
auto frame_buffer(picture_reference<I> const& p) -> decltype(frame_buffer(*p.frame)) {
  return frame_buffer(*p.frame);
}

template<typename I, typename P>
picture_reference<I> find_picture(picture_type curr_pic_type, I first_frame, I last_frame, P predicate) {
  for(;first_frame != last_frame; ++first_frame) {
    if(curr_pic_type == picture_type::frame) {
      if(predicate({first_frame, picture_type::frame})) return {first_frame, picture_type::frame};  
    } else {
      if(predicate({first_frame, picture_type::top})) return {first_frame, picture_type::top};
      if(predicate({first_frame, picture_type::bot})) return {first_frame, picture_type::bot};
    }
  }

  return {first_frame, curr_pic_type == picture_type::frame ? picture_type::frame : picture_type::top};
}

template<typename I, typename O>
void for_each_picture(picture_type curr_pic_type, I first_frame, I last_frame, O op) {
  find_picture(curr_pic_type, first_frame, last_frame, [&](picture_reference<I> const& p) { 
    op(p);
    return false;
  });
}

template<typename I, typename O>
void for_each_picture(I first_frame, I last_frame, O op) {
  for(;first_frame != last_frame; ++first_frame) {
    op(picture_reference<I>{first_frame, picture_type::frame});
    op(picture_reference<I>{first_frame, picture_type::top});
    op(picture_reference<I>{first_frame, picture_type::bot});
  }
}

template<typename I, typename C>
picture_reference<I> get_picture_by_picnum(unsigned max_frame_num, C& curr_pic, I first_frame, I last_frame, int picnum) {
  return find_picture(pic_type(curr_pic), first_frame, last_frame,
    [&](picture_reference<I> const& p) { return is_short_term_reference(p) && picnum == PicNum(max_frame_num, curr_pic, p); });
}

template<typename I, typename C>
picture_reference<I> get_picture_by_long_term_picnum(C const& curr_pic, I first_frame, I last_frame, unsigned long_term_picnum) {
  return find_picture(pic_type(curr_pic), first_frame, last_frame,
    [&](picture_reference<I> const& p) { return is_long_term_reference(p) && long_term_picnum == LongTermPicNum(curr_pic, p);});
}

template<typename I>
picture_reference<I> const& check_bounds(picture_reference<I> const& ref, I first_frame, I last_frame) {
  if(ref.frame == last_frame) throw std::runtime_error("invalid picture reference");
  return ref;
}

// 8.2.4.3 Modification process for reference picture lists
template<typename I, typename C, typename R, typename M>
R ref_pic_list_modification(
  unsigned max_frame_num, C const& curr_pic,
  I first_frame, I last_frame,
  M first_modification, M last_modification,
  R first_reference, R last_reference)
{
  auto picNumLXPred = PicNum(curr_pic);
  auto short_term = [&](ref_pic_list_modification_operation const& op) mutable {
    picNumLXPred += op.id == 0 ? -(op.abs_diff_pic_num_minus1 + 1) : (op.abs_diff_pic_num_minus1 + 1);
    picNumLXPred = (picNumLXPred + MaxPicNum(max_frame_num, curr_pic)) % MaxPicNum(max_frame_num, curr_pic);
    auto picnum =  picNumLXPred > PicNum(curr_pic) ? picNumLXPred - MaxPicNum(max_frame_num, curr_pic) : picNumLXPred;
    return get_picture_by_picnum(max_frame_num, curr_pic, first_frame, last_frame,  picnum);
  };

  auto modify = [&](R first, R last, picture_reference<I> r) {
    picture_reference<I> c = r;
    for(;first != last; ++first) {
      std::swap(c, *first);
      if(c == r) break;
    }
  };

  for(;first_reference != last_reference && first_modification != last_modification; ++first_reference, ++first_modification) {
    try {
    auto r = first_modification->id != 2 ?
        short_term(*first_modification) : get_picture_by_long_term_picnum(curr_pic, first_frame, last_frame, first_modification->long_term_pic_num);
    check_bounds(r, first_frame, last_frame);
    modify(first_reference, last_reference, r);
    } catch(std::exception const& e) {
      std::cerr << e.what() << std::endl;
    }
  }
  return first_reference;
}

// 8.2.4 Decoding process for reference picture lists construction
template<typename K, typename I, typename O>
O initialise_reflist(picture_type curr_pic_type, K key, I first_frame, I last_frame, O first, O last) {
  auto last_short_term = first;
  find_picture(curr_pic_type, first_frame, last_frame, [&](picture_reference<I> const& p) { 
    if(last_short_term == last) return true;
    if(is_short_term_reference(p)) *last_short_term++ = p;
    return false;
  });
 
  std::sort(first, last_short_term, [&](picture_reference<I> const& a, picture_reference<I> const& b) { return key(a) < key(b); });
  utils::stable_alternate(first, last_short_term, [&](picture_reference<I> const& p) { return pic_type(p) == curr_pic_type; });
 
  auto last_long_term = last_short_term;
  find_picture(curr_pic_type, first_frame, last_frame, [&](picture_reference<I> const& p) {
    if(last_long_term == last) return true;
    if(is_long_term_reference(p)) *last_long_term++ = p;
    return false;
  });
  std::sort(last_short_term, last_long_term,  [&](picture_reference<I> const& a, picture_reference<I> const& b) { return LongTermFrameIdx(a) < LongTermFrameIdx(b); });
  utils::stable_alternate(last_short_term, last_long_term, [&](picture_reference<I> const& p) { return pic_type(p) == curr_pic_type; });

  return last_long_term;
}

template<typename C, typename I, typename M, typename O>
O generate_reflist_for_p_slice(unsigned max_frame_num, C const& curr_pic,
  utils::range<I> frames,
  utils::range<M> modifications,
  O output, std::size_t num_ref_idx_l0_active) 
{
  std::array<picture_reference<I>,32> ref = {{}};
  num_ref_idx_l0_active = std::min(num_ref_idx_l0_active, ref.size());

  auto i = initialise_reflist(pic_type(curr_pic), [&](picture_reference<I> const& p) { return -FrameNumWrap(max_frame_num, curr_pic, p); },
                              begin(frames), end(frames), begin(ref), end(ref));

  auto r = ref_pic_list_modification(max_frame_num, curr_pic, begin(frames), end(frames), begin(modifications), end(modifications), begin(ref), end(ref));
  
  return std::copy(begin(ref), std::min(begin(ref) + num_ref_idx_l0_active, std::max(i,r)), output);
}

template<typename C, typename I, typename M, typename O>
std::pair<O,O> generate_reflists_for_b_slice(unsigned max_frame_num, C const& curr_pic,
  utils::range<I> frames,
  utils::range<M> modifications0, utils::range<M> modifications1,
  O output0, std::size_t num_ref_idx_l0_active,
  O output1, std::size_t num_ref_idx_l1_active) 
{
  std::array<picture_reference<I>,32> ref0={{}}, ref1={{}};

  num_ref_idx_l0_active = std::min(num_ref_idx_l0_active, ref0.size());
  num_ref_idx_l1_active = std::min(num_ref_idx_l1_active, ref1.size());

  auto key0 = [&](picture_reference<I> const& a) {
    return ((PicOrderCnt(a) <= PicOrderCnt(curr_pic)) ? std::make_tuple(false, -PicOrderCnt(a)-1) : std::make_tuple(true, PicOrderCnt(a)+1)); 
  };
  auto i0 = initialise_reflist(pic_type(curr_pic), key0, begin(frames), end(frames), begin(ref0), end(ref0)); 

  auto key1 = [&](picture_reference<I> const& a) {
    return ((PicOrderCnt(a) > PicOrderCnt(curr_pic)) ? std::make_tuple(false, PicOrderCnt(a)+1) : std::make_tuple(true, -PicOrderCnt(a)-1));
  };
  auto i1 = initialise_reflist(pic_type(curr_pic), key1, begin(frames), end(frames), begin(ref1), end(ref1)); 

  if(num_ref_idx_l1_active > 1 && num_ref_idx_l0_active == num_ref_idx_l1_active && std::equal(begin(ref0), begin(ref0) + num_ref_idx_l0_active, begin(ref1)))
    std::swap(ref1[0], ref1[1]);

  auto r0 = ref_pic_list_modification(max_frame_num, curr_pic, begin(frames), end(frames), begin(modifications0), end(modifications0), begin(ref0), end(ref0));
  auto r1 = ref_pic_list_modification(max_frame_num, curr_pic, begin(frames), end(frames), begin(modifications1), end(modifications1), begin(ref1), end(ref1));

  return std::make_pair(std::copy(begin(ref0), std::min(begin(ref0) + num_ref_idx_l0_active, std::max(i0, r0)), output0), 
                        std::copy(begin(ref1), std::min(begin(ref1) + num_ref_idx_l1_active, std::max(i1, r1)), output1));
}

template<typename I, typename C>
void mark_as_long_term_reference(I first_frame, I last_frame, C& pic, unsigned long_term_frame_idx) {
 for_each_picture(first_frame, last_frame, [&](picture_reference<I> const& p) {
    if(is_long_term_reference(p) && LongTermFrameIdx(p) == long_term_frame_idx && !(pic_type(pic) == opposite(pic_type(p)) && FrameNum(p) == FrameNum(pic)))
      mark_as_unused_for_reference(p);
  });

  mark_as_long_term_reference(pic, long_term_frame_idx);
}

// 8.2.5.4 Adaptive memory control decoded reference picture marking process
template<typename I, typename C>
void adaptive_memory_control(unsigned max_frame_num, memory_management_control_operation const& op, C& curr_pic, I first_frame, I last_frame) {
  if(op.id == 1) { 
    auto picture = get_picture_by_picnum(max_frame_num, curr_pic, first_frame, last_frame, PicNum(curr_pic) - (op.difference_of_pic_nums_minus1 + 1));
    check_bounds(picture, first_frame, last_frame); 
    mark_as_unused_for_reference(picture);
  }
  else if(op.id == 2)
    mark_as_unused_for_reference(check_bounds(get_picture_by_long_term_picnum(curr_pic, first_frame, last_frame, op.long_term_pic_num), first_frame, last_frame));
  else if(op.id == 3) { 
    auto picture = get_picture_by_picnum(max_frame_num, curr_pic, first_frame, last_frame, PicNum(curr_pic) - (op.difference_of_pic_nums_minus1 + 1)); 
    check_bounds(picture, first_frame, last_frame);
    mark_as_long_term_reference(first_frame, last_frame, picture, op.long_term_frame_idx);
  }
  else if(op.id == 4) {
    for_each_picture(first_frame, last_frame, [&](picture_reference<I> const& a) {
      if(is_long_term_reference(a) &&  LongTermFrameIdx(a) >= op.max_long_term_frame_idx_plus1)
        mark_as_unused_for_reference(a);
    });
  }
  else if(op.id == 5) {
    while(first_frame != last_frame) mark_as_unused_for_reference(*first_frame++);
    
    FrameNum(curr_pic, 0);
  
    auto tempPicOrderCnt = PicOrderCnt(curr_pic);
    if(has_top(pic_type(curr_pic))) TopFieldOrderCnt(curr_pic, TopFieldOrderCnt(curr_pic) - tempPicOrderCnt);
    if(has_bot(pic_type(curr_pic))) BotFieldOrderCnt(curr_pic, BotFieldOrderCnt(curr_pic) - tempPicOrderCnt);
  }
  else if(op.id == 6) {
    mark_as_long_term_reference(first_frame, last_frame, curr_pic, op.long_term_frame_idx);
  }
}

// 8.2.5.3 Sliding window decoded reference picture marking process
template<typename C, typename I>
void dec_ref_pic_marking_sliding_window(unsigned max_num_ref_frames, C& curr_pic, I begin, I end) {
  int numShortTerm = 0, numLongTerm = 0;
  for(auto b = begin, e = end; b != e; ++b) {
    if((pic_type(curr_pic) == picture_type::top && is_short_term_reference(bot(*b)) && FrameNum(*b) == FrameNum(curr_pic))
      || (pic_type(curr_pic) == picture_type::bot && is_short_term_reference(top(*b)) && FrameNum(*b) == FrameNum(curr_pic)))
      continue;

    if(is_short_term_reference(*b) || is_short_term_reference(top(*b)) || is_short_term_reference(bot(*b))) ++numShortTerm;
    else if(is_long_term_reference(*b) || is_long_term_reference(top(*b)) || is_long_term_reference(bot(*b))) ++numLongTerm;
  }

  int n = std::min(numShortTerm + numLongTerm - std::max(int(max_num_ref_frames), 1), numShortTerm);
  
  for(auto b = begin, e = end; b != e && n >= 0; ++b) {
    if(is_short_term_reference(*b)) {
      mark_as_unused_for_reference(*b);
      --n;
    }
  }
}

// 8.2.5 Decoded reference picture marking process
struct dec_ref_pic_marker {
  unsigned max_frame_num;
  std::uint8_t max_num_ref_frames;
  std::uint8_t nal_ref_idc;
  bool IdrPicFlag;
  bool long_term_reference_flag;

  std::vector<memory_management_control_operation> mmcos;

  dec_ref_pic_marker() {}
  dec_ref_pic_marker(seq_parameter_set const& sps, slice_header&& slice) :
    max_frame_num(1 << (sps.log2_max_frame_num_minus4 + 4)),
    max_num_ref_frames(sps.max_num_ref_frames),
    nal_ref_idc(slice.nal_ref_idc),
    IdrPicFlag(slice.IdrPicFlag),
    long_term_reference_flag(slice.long_term_reference_flag),
    mmcos(std::move(slice.mmcos)) 
  {}

  template<typename Picture, typename Iterator>
  void operator()(Picture& curr_pic, Iterator begin, Iterator end) {
    if(nal_ref_idc == 0) return;

    if(IdrPicFlag) {
      while(begin != end) mark_as_unused_for_reference(*begin++);

      if(long_term_reference_flag)
        mark_as_long_term_reference(curr_pic, 0);
      else
        mark_as_short_term_reference(curr_pic);
    }
    else {
      if(mmcos.empty())
        dec_ref_pic_marking_sliding_window(max_num_ref_frames, curr_pic, begin, end);
      else {
        for(auto& op: mmcos)
          try {
            adaptive_memory_control(max_frame_num, op, curr_pic, begin, end);
          }
          catch(std::exception const& e) {
            std::cerr << e.what() << std::endl;
          }
      }
      if(!is_long_term_reference(curr_pic)) mark_as_short_term_reference(curr_pic);
    }
  }
};

// C.4.4 Removal of pictures from the DPB before possible insertion of the current picture
template<typename I>
I remove_unused_pictures(I begin, I end) {
  return std::remove_if(begin, end, [](decltype(*begin) v) { return !is_needed_for_output(v) && !is_short_term_reference(v) && !is_long_term_reference(v); });
}

} // namespace h264
} // namespace media

#endif
