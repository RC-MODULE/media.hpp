#include "msvd.hpp"
#include <linux/msvdhd.h>
#include <iomanip>

namespace msvd {

enum status : std::uint32_t {
  mvde_cmd_rdy = 1 << 0, 
  mvde_pic_rdy = 1 << 1,
  mvde_last_mb_rdy = 1 << 2,
  mvde_mvde_proc_delay = 1 << 3,
  mvde_proc_err = 1 << 4,
  sr1_cmd_rdy = 1 << 16,
  sr1_si_access_failed =  1 << 17,
  sr1_start_code_found = 1 << 18,
  sr1_pes_hd_found = 1 << 19,
  sr1_ts_extracted = 1 << 20,
  sr1_str_buf_changed =  1 << 21,
  sr1_str_buf_thresh = 1 << 22,
  sr1_str_buf_empty = 1 << 23,
  sr1_read_fail = 1 << 24,
  sr1_offs_end_missed = 1 << 25,
  sr1_offs_end_reached = 1 << 26,
  si_cmd_rdy = 1 << 27,
  si_start_dec = 1 << 28,
  si_parse_err = 1 << 29,
  si_data_err = 1 << 30
};

enum class si_cmd : std::uint32_t {
  nop = 0,
  reset = 1,
  suspend_bus_transfers = 2,
  decode_until_return_point = 3,
  conceal_num_macroblocks = 4,
  remove_num_macroblocks = 5,
  stop_decoding = 6,
  release_bus_transfers = 7
};

enum class mvde_cmd : std::uint32_t {
  nop = 0,
  reset = 1,
  suspend_bus_transfers = 2,
  start_continuous_decoding = 3,
  start_macroblock_decoding = 4,
  abort = 5,
  start_decode_until_return_point = 6,
  release_bus_transfers = 7
};

struct ref_pic_info_tbl_t {
  std::uint32_t d[32];
};

struct weighted_pred_tables {
  struct {
    std::uint32_t luma[16];
    std::uint32_t chroma[32];
  } offs[2];
  
  struct {
    std::uint32_t luma;
    std::uint32_t chroma;
  } flags[2];
};

struct register_map {
  uint32_t vd_clc; // Clock control (rwhh) MSVD_BASE + 0x0000
  uint32_t vd_id; // Revision identification (r) MSVD_BASE + 0x0004
  uint32_t vd_imsc; // Interrupt mask (rw) MSVD_BASE + 0x0008
  uint32_t vd_ris; // Raw interrupt status (r) MSVD_BASE + 0x000c
  uint32_t vd_mis; // Masked interrupt status (r) MSVD_BASE + 0x0010
  uint32_t vd_icr; // Interrupt status clear (w) MSVD_BASE + 0x0014
  uint32_t vd_isr; // Interrupt status set (w) MSVD_BASE + 0x0018
  uint32_t _notused_0 [(0x8000-0x001c)/4]; // gap in address space
  mvde_cmd vdmv_cmd;//uint32_t vdmv_cmd; // Command register of MVDE (w) MSVD_BASE + 0x8000
  uint32_t vdmv_stat; // Processing status of MVDE (r) MSVD_BASE + 0x8004
  uint32_t vdmv_ma_stat; // Status of memory access units of MVDE (r) MSVD_BASE + 0x8008
  uint32_t vdmv_cp_error; // MVDE error status (r) MSVD_BASE + 0x800c
  uint32_t vdmv_param_0; // Context header parameters (general and CP related) (rwhh) MSVD_BASE + 0x8010
  uint32_t vdmv_param_1; // Picture / Slice parameters (general and IP related) (rwhh) MSVD_BASE + 0x8014
  uint32_t vdmv_param_2; // Picture / Slice parameters (for CP and MC) (rwhh) MSVD_BASE + 0x8018
  uint32_t vdmv_param_3; // Picture / Slice parameters (for MC) (rwhh) MSVD_BASE + 0x801c
  uint32_t vdmv_param_4; // Picture / Slice parameters (for MC) (rwhh) MSVD_BASE + 0x8020
  uint32_t vdmv_param_5; // Picture / Slice parameters (for MC) (rwhh) MSVD_BASE + 0x8024
  uint32_t vdmv_param_6; // Picture / Slice parameters (for MC) (rwhh) MSVD_BASE + 0x8028
  uint32_t vdmv_param_7; // Picture / Slice parameters (for OS, LF and MC_MA) (rwhh) MSVD_BASE + 0x802c
  uint32_t vdmv_delay_thresh; // Threshold for delay detection (rw) MSVD_BASE + 0x8030
  uint32_t vdmv_ncyc_avg_mb; // Average number of cycles per macroblock (rw) MSVD_BASE + 0x8034
  uint32_t vdmv_delay_stat; // Processing delay status (r) MSVD_BASE + 0x8038
  uint32_t vdmv_ncyc1_pic; // Total cycles per picture (r) MSVD_BASE + 0x803c
  uint32_t vdmv_ncyc2_pic; // Effective cycles per picture (r) MSVD_BASE + 0x8040
  uint32_t vdmv_ncyc_max_mb; // Maximum number of cycles per macroblock (r) MSVD_BASE + 0x8044
  uint32_t vdmv_pos_max_mb; // Position of macroblock with maximum number of cycles (r) MSVD_BASE + 0x8048
  uint32_t vdmv_cp_buf_ba; // CP coefficient buffer base address (rwhh) MSVD_BASE + 0x804c
  uint32_t vdmv_ip_buf_ba; // IP line buffer base address (rwhh) MSVD_BASE + 0x8050
  uint32_t vdmv_os_buf_ba; // OS line buffer base address (rwhh) MSVD_BASE + 0x8054
  uint32_t vdmv_os_frame_store_y_ba; // OS luminance frame store base address (rwhh) MSVD_BASE + 0x8058
  uint32_t vdmv_os_frame_store_c_ba; // OS chrominance frame store base address (rwhh) MSVD_BASE + 0x805c
  uint32_t vdmv_lf_buf_ba; // LF line buffer base address (rwhh) MSVD_BASE + 0x8060
  uint32_t vdmv_lf_frame_store_y_ba; // LF luminance frame store base address (rwhh) MSVD_BASE + 0x8064
  uint32_t vdmv_lf_frame_store_c_ba; // LF chrominance frame store base address (rwhh) MSVD_BASE + 0x8068
  uint32_t _notused_1 [(0x8100-0x806c)/4]; // gap in address space
  uint32_t vdmv_frame_store_y_ba[32]; // base address of luminance frame store for MC (rwhh) 0x8100+n (n=0..31)
  uint32_t vdmv_frame_store_c_ba[32]; // base address of chrominance frame store for MC (rwhh) 0x8180+n (n=0..31)
  uint32_t vdmv_cp_data[128]; // CP RAM (rwhh) 0x8200+n (n=0..127)
  uint32_t _notused_2 [(0x10000-0x8400)/4]; // gap in address space
  uint32_t vdsr1_cmd; // Command register of SR1 (w) MSVD_BASE + 0x10000
  uint32_t vdsr1_stat; // Processing status of SR1 (r) MSVD_BASE + 0x10004
  uint32_t vdsr1_pes_hdr_data; // PES header data (r) MSVD_BASE + 0x10008
  uint32_t vdsr1_pts_l; // Presentation time stamp (r) MSVD_BASE + 0x1000c
  uint32_t vdsr1_dts_l; // Decoding time stamp (r) MSVD_BASE + 0x10010
  uint32_t vdsr1_search_range; // Search range for search commands (rw) MSVD_BASE + 0x10014
  uint32_t vdsr1_search_pattern_1; // Search pattern no. 1 (rw) MSVD_BASE + 0x10018
  uint32_t vdsr1_search_mask_1; // Search mask no. 1 (rw) MSVD_BASE + 0x1001c
  uint32_t vdsr1_cfg; // Configuration of SR1 (rw) MSVD_BASE + 0x10020
  uint32_t vdsr1_str_buf_ba; // Stream buffer base address (rw) MSVD_BASE + 0x10024
  uint32_t vdsr1_str_buf_size; // Stream buffer size (rw) MSVD_BASE + 0x10028
  uint32_t vdsr1_str_buf_blen; // Stream buffer block length (rw) MSVD_BASE + 0x1002c
  uint32_t vdsr1_str_buf_thresh; // Stream buffer threshold (rw) MSVD_BASE + 0x10030
  uint32_t vdsr1_str_offs; // Stream offset (rwhh) MSVD_BASE + 0x10034
  uint32_t vdsr1_str_offs_end; // Stream offset end (rw) MSVD_BASE + 0x10038
  uint32_t vdsr1_str_buf_rdptr; // Stream buffer read pointer (r) MSVD_BASE + 0x1003c
  uint32_t vdsr1_search_pattern_2; // Search pattern no. 2 (rw) MSVD_BASE + 0x10040
  uint32_t vdsr1_search_mask_2; // Search mask no. 2 (rw) MSVD_BASE + 0x10044
  uint32_t vdsr1_parse_stat; // Parse status of SR1 (rwhh) MSVD_BASE + 0x10048
  uint32_t _notused_3 [(0x10100-0x1004c)/4]; // gap in address space
  uint32_t vdsr1_get_bits[32]; // Stream data bits (get_bits) (r) 0x10100+n (n=0..31)
  uint32_t vdsr1_show_bits[32]; // Stream data bits (show_bits) (r) 0x10180+n (n=0..31)
  uint32_t vdsr1_flush_show[32]; // Stream data bits (flush_show) (r) 0x10200+n (n=0..31)
  uint32_t vdsr1_show_aligned; // Stream data from next byte boundary (r) MSVD_BASE + 0x10280
  uint32_t vdsr1_show_aligned_em; // Stream data with emulation bytes from next byte boundary (r) MSVD_BASE + 0x10284
  uint32_t _notused_4 [(0x10400-0x10288)/4]; // gap in address space
  uint32_t vdsr2_cmd; // Command register of SR2 (w) MSVD_BASE + 0x10400
  uint32_t vdsr2_stat; // Processing status of SR2 (r) MSVD_BASE + 0x10404
  uint32_t vdsr2_pes_hdr_data; // PES header data (r) MSVD_BASE + 0x10408
  uint32_t vdsr2_pts_l; // Presentation time stamp (r) MSVD_BASE + 0x1040c
  uint32_t vdsr2_dts_l; // Decoding time stamp (r) MSVD_BASE + 0x10410
  uint32_t vdsr2_search_range; // Search range for search commands (rw) MSVD_BASE + 0x10414
  uint32_t vdsr2_search_pattern_1; // Search pattern no. 1 (rw) MSVD_BASE + 0x10418
  uint32_t vdsr2_search_mask_1; // Search mask no. 1 (rw) MSVD_BASE + 0x1041c
  uint32_t vdsr2_cfg; // Configuration of SR2 (rw) MSVD_BASE + 0x10420
  uint32_t vdsr2_str_buf_ba; // Stream buffer base address (rw) MSVD_BASE + 0x10424
  uint32_t vdsr2_str_buf_size; // Stream buffer size (rw) MSVD_BASE + 0x10428
  uint32_t vdsr2_str_buf_blen; // Stream buffer block length (rw) MSVD_BASE + 0x1042c
  uint32_t vdsr2_str_buf_thresh; // Stream buffer threshold (rw) MSVD_BASE + 0x10430
  uint32_t vdsr2_str_offs; // Stream offset (rwhh) MSVD_BASE + 0x10434
  uint32_t vdsr2_str_offs_end; // Stream offset end (rw) MSVD_BASE + 0x10438
  uint32_t vdsr2_str_buf_rdptr; // Stream buffer read pointer (r) MSVD_BASE + 0x1043c
  uint32_t vdsr2_search_pattern_2; // Search pattern no. 2 (rw) MSVD_BASE + 0x10440
  uint32_t vdsr2_search_mask_2; // Search mask no. 2 (rw) MSVD_BASE + 0x10444
  uint32_t vdsr2_parse_stat; // Parse status of SR2 (rwhh) MSVD_BASE + 0x10448
  uint32_t _notused_5 [(0x10500-0x1044c)/4]; // gap in address space
  uint32_t vdsr2_get_bits[32]; // Stream data bits (get_bits) (r) 0x10500+n (n=0..31)
  uint32_t vdsr2_show_bits[32]; // Stream data bits (show_bits) (r) 0x10580+n (n=0..31)
  uint32_t vdsr2_flush_show[32]; // Stream data bits (flush_show) (r) 0x10600+n (n=0..31)
  uint32_t vdsr2_show_aligned; // Stream data from next byte boundary (r) MSVD_BASE + 0x10680
  uint32_t vdsr2_show_aligned_em; // Stream data with emulation bytes from next byte boundary (r) MSVD_BASE + 0x10684
  uint32_t _notused_6 [(0x10800-0x10688)/4]; // gap in address space
  si_cmd   vdsi_cmd;//uint32_t vdsi_cmd; // Command register of SI (w) MSVD_BASE + 0x10800
  uint32_t vdsi_stat; // Status register of SI (r) MSVD_BASE + 0x10804
  uint32_t vdsi_error_stat; // SI error status (r) MSVD_BASE + 0x10808
  uint32_t vdsi_cfg; // Configuration of SI (rw) MSVD_BASE + 0x1080c
  uint32_t vdsi_dma_list0_ad; // DMA list 0 start address (rw) MSVD_BASE + 0x10810
  uint32_t vdsi_dma_list1_ad; // DMA list 1 start address (rw) MSVD_BASE + 0x10814
  uint32_t vdsi_dma_list2_ad; // DMA list 2 start address (rw) MSVD_BASE + 0x10818
  uint32_t vdsi_conceal_cfg; // Concealment configuration (rw) MSVD_BASE + 0x1081c
  uint32_t vdsi_conceal_cnt; // Concealment count (r) MSVD_BASE + 0x10820
  uint32_t vdsi_mod_stat; // Status of SI modules (r) MSVD_BASE + 0x10824
  uint32_t vdsi_param_1; // Sequence / Picture parameters (for SI) (rw) MSVD_BASE + 0x10828
  uint32_t vdsi_param_2; // Sequence / Picture parameters (for SI) (rw) MSVD_BASE + 0x1082c
  uint32_t vdsi_param_3; // Sequence / Picture parameters (for SI) (rw) MSVD_BASE + 0x10830
  uint32_t vdsi_param_4; // Sequence / Picture parameters (for SI) (rw) MSVD_BASE + 0x10834
  uint32_t vdsi_param_5; // Sequence / Picture parameters (for SI) (rw) MSVD_BASE + 0x10838
  uint32_t vdsi_param_6; // General parameters (for SI and MVDE) (rw) MSVD_BASE + 0x1083c
  uint32_t vdsi_param_7; // Picture / Slice parameters (for MVDE) (rw) MSVD_BASE + 0x10840
  uint32_t vdsi_param_8; // Picture / Slice parameters (for MVDE) (rw) MSVD_BASE + 0x10844
  uint32_t vdsi_param_9; // Picture / Slice parameters (for MVDE) (rw) MSVD_BASE + 0x10848
  uint32_t vdsi_param_10; // Picture / Slice parameters (for MVDE) (rw) MSVD_BASE + 0x1084c
  uint32_t vdsi_param_11; // Picture / Slice parameters (for MVDE) (rw) MSVD_BASE + 0x10850
  uint32_t vdsi_param_12; // Picture / Slice parameters (for MVDE) (rw) MSVD_BASE + 0x10854
  uint32_t vdsi_pe_nsuma_ba; // PE_NSUMA buffer base address (rwhh) MSVD_BASE + 0x10858
  uint32_t vdsi_bpma_ba; // BPMA buffer base address (rwhh) MSVD_BASE + 0x1085c
  uint32_t vdsi_su_nsuma_ba; // SU_NSUMA buffer base address (rwhh) MSVD_BASE + 0x10860
  uint32_t vdsi_psuma_rd_top_ba; // PSUMA read buffer top field base address (rwhh) MSVD_BASE + 0x10864
  uint32_t vdsi_psuma_rd_bot_ba; // PSUMA read buffer bottom field base address (rwhh) MSVD_BASE + 0x10868
  uint32_t vdsi_psuma_wr_top_ba; // PSUMA write buffer top field base address (rwhh) MSVD_BASE + 0x1086c
  uint32_t vdsi_psuma_wr_bot_ba; // PSUMA write buffer bottom field base address (rwhh) MSVD_BASE + 0x10870
  uint32_t vdsi_fifo_error; // SI_FF FIFO error (rcw) MSVD_BASE + 0x10874
  uint32_t vdsi_fifo_fill_level; // Fill level of FIFOs (r) MSVD_BASE + 0x10878
  uint32_t vdsi_fifo_dma_stat; // 
  uint32_t vdsi_fifo_clear; // Clear FIFOs (w) MSVD_BASE + 0x10880
  uint32_t vdsi_fifo_mb_ctxt; // Macroblock context FIFO data port (w) MSVD_BASE + 0x10884
  uint32_t vdsi_fifo_mv; // Motion vector FIFO data port (w) MSVD_BASE + 0x10888
  uint32_t vdsi_fifo_coeff; // Coefficient FIFO data port (w) MSVD_BASE + 0x1088c
  uint32_t vdsi_fifo_dma_cmd; // DMA command (w) MSVD_BASE + 0x10890
  uint32_t vdsi_fifo_dma_ad; // DMA address configuration (rw) MSVD_BASE + 0x10894
  uint32_t _notused_7 [(0x10900-0x10898)/4]; // gap in address space
  
  ref_pic_info_tbl_t ref_pic_info_tbl;
  std::uint32_t cnt_tbl_l[2][32];

  weighted_pred_tables wt;

  uint32_t vdsi_rpi_data_rest[(0x11100-0x10900)/4 - 196]; // RPI RAM (rwhh) 0x10900+n (n=0..511)
  
  uint32_t vdsi_pe_nsuma_start_ad; // PE_NSUMA buffer start address (rwhh) MSVD_BASE + 0x11100
  uint32_t vdsi_pe_nsuma_end_ad; // PE_NSUMA buffer end address (rwhh) MSVD_BASE + 0x11104
  uint32_t vdsi_su_nsuma_start_ad; // SU_NSUMA buffer start address (rwhh) MSVD_BASE + 0x11108
  uint32_t vdsi_su_nsuma_end_ad; // SU_NSUMA buffer end address (rwhh) MSVD_BASE + 0x1110c
  uint32_t vdsi_bpma_start_ad; // BPMA buffer offset address (rwhh) MSVD_BASE + 0x11110
  uint32_t vdsi_bpma_start_pos; // BPMA buffer current position (rwhh) MSVD_BASE + 0x11114
  uint32_t _notused_8 [(0x11200-0x11118)/4]; // gap in address space
  uint32_t vdsi_su_cmd; // Command register of SI_SU (w) MSVD_BASE + 0x11200
  uint32_t vdsi_su_ctxt_data_0; // context data register 0 (rwhh) MSVD_BASE + 0x11204
  uint32_t vdsi_su_ctxt_data_1; // context data register 1 (rwhh) MSVD_BASE + 0x11208
  uint32_t _notused_9 [(0x11300-0x1120c)/4]; // gap in address space
  uint32_t vdsi_cabac_data[128]; // CABAC RAM (rwhh) 0x11300+n (n=0..127)
  uint32_t vdsi_pe_cmd; // Command register of SI_PE (w) MSVD_BASE + 0x11500
  uint32_t vdsi_pe_ctx1; // context data register 1 (rwhh) MSVD_BASE + 0x11504
  uint32_t vdsi_pe_ctx2; // context data register 2 (rwhh) MSVD_BASE + 0x11508
  uint32_t vdsi_pe_ctx3; // context data register 3 (rwhh) MSVD_BASE + 0x1150c
  uint32_t _notused_10 [(0x18000-0x11510)/4]; // gap in address space
  uint32_t vdsi_cfg_data[256]; // MSVD_HD setup data (rwhh) 0x18000+n (n=0..255)
};

std::ostream& operator << (std::ostream& os, std::pair<volatile std::uint32_t*, int> v) {
  os << "{";
  for(int i = 0; i != v.second; ++i)
    os << v.first[i] << std::endl;
  return os << "}";
}

std::ostream& operator << (std::ostream& os, buffer_geometry const& g) {
  return os << "{width:" << std::dec << g.width << ",height:" << g.height << 
    std::hex << ",luma_offset:" << g.luma_offset << ",chroma_offset:" << g.chroma_offset << "}";
}

std::ostream& operator << (std::ostream& os, mpeg::picture_coding t) {
  return os << (t == mpeg::picture_coding::I ? "I" : (t == mpeg::picture_coding::P ? "P" : "B"));
}
 
std::ostream& operator << (std::ostream& os, mpeg::picture_header_t const& ph) {
  return std::cout <<  "{"
    << "\n\ttemporal_reference:" << ph.temporal_reference
    << "\n\tpicture_coding_type:" << ph.picture_coding_type
    << "\n\tvbv_delay:" << ph.vbv_delay
    << "\n\tfull_pel_forward_vector:" << ph.full_pel_forward_vector
    << "\n\tforward_f_code:" << ph.forward_f_code
    << "\n\tfull_pel_backward_vector:" << ph.full_pel_backward_vector
    << "\n\tbackward_f_code:" << ph.backward_f_code
    << "\n}";
}

std::ostream& operator << (std::ostream& os, mpeg::picture_coding_extension_t const& pcx) {
  return std::cout << "{"
    << "\n\tf_code: {" << pcx.f_code[0][0] << "," << pcx.f_code[0][1] << "," << pcx.f_code[1][0] << "," << pcx.f_code[1][1] << "}"
    << "\n\tintra_dc_precision:" << pcx.intra_dc_precision 
    << "\n\tpicture_structure:" << pcx.picture_structure 
    << "\n\ttop_field_first:" << pcx.top_field_first 
    << "\n\tframe_pred_frame_dct:" << pcx.frame_pred_frame_dct
    << "\n\tconcealment_motion_vectors:" << pcx.concealment_motion_vectors
    << "\n\tq_scale_type:" << pcx.q_scale_type
    << "\n\tintra_vlc_format:" << pcx.intra_vlc_format
    << "\n\talternate_scan:" << pcx.alternate_scan
    << "\n\trepeat_first_field:" << pcx.repeat_first_field
    << "\n\tchroma_420_type:" << pcx.chroma_420_type
    << "\n\tprogressive_frame:" << pcx.progressive_frame
    << "\n\tcomposite_display_flag:" << pcx.composite_display_flag
    << "\n\tv_axis:" << pcx.v_axis
    << "\n\tfield_sequence:" << pcx.field_sequence
    << "\n\tsub_carrier:" << pcx.sub_carrier
    << "\n\tburst_amplitude:" << pcx.burst_amplitude 
    << "\n\tsub_carrier_phase:" << pcx.sub_carrier_phase
    << "\n}";
}

std::ostream& dump_buffer64(std::ostream& is, utils::range<volatile std::uint64_t*> r) {
  for(auto i = begin(r); i != end(r);++i)
    std::cout << std::uint32_t(i) << " " << std::hex << std::setw(16) << std::setfill('0') << *i << std::endl;
}


std::ostream& dump_sr1_regs(std::ostream& os, register_map volatile* regs) {
  return os << std::hex 
    << "vdsr1_cmd:" << regs->vdsr1_cmd << std::endl
    << "vdsr1_stat:" << regs->vdsr1_stat << std::endl
    << "vdsr1_pes_hdr_data:" << regs->vdsr1_pes_hdr_data << std::endl
    << "vdsr1_pts_l:" << regs->vdsr1_pts_l << std::endl
    << "vdsr1_dts_l:" << regs->vdsr1_dts_l << std::endl
    << "vdsr1_search_range:" << regs->vdsr1_search_range << std::endl
    << "vdsr1_search_pattern_1:" << regs->vdsr1_search_pattern_1 << std::endl
    << "vdsr1_search_mask_1:" << regs->vdsr1_search_mask_1 << std::endl
    << "vdsr1_cfg:" << regs->vdsr1_cfg << std::endl
    << "vdsr1_str_buf_ba:" << regs->vdsr1_str_buf_ba << std::endl
    << "vdsr1_str_buf_size:" << regs->vdsr1_str_buf_size << std::endl
    << "vdsr1_str_buf_blen:" << regs->vdsr1_str_buf_blen << std::endl
    << "vdsr1_str_buf_thresh:" << regs->vdsr1_str_buf_thresh << std::endl
    << "vdsr1_str_offs:" << regs->vdsr1_str_offs << std::endl
    << "vdsr1_str_offs_end:" << regs->vdsr1_str_offs_end << std::endl
    << "vdsr1_str_buf_rdptr:" << regs->vdsr1_str_buf_rdptr << std::endl
    << "vdsr1_search_pattern_2:" << regs->vdsr1_search_pattern_2 << std::endl
    << "vdsr1_search_mask_2:" << regs->vdsr1_search_mask_2 << std::endl
    << "vdsr1_parse_stat:" << regs->vdsr1_parse_stat << std::endl
  ;
}

std::ostream& dump_stats_regs(std::ostream& os, register_map volatile* regs) {
  return os << std::hex
    << "vd_ris:" << regs->vd_ris << std::endl
    << "vd_isr:" << regs->vd_isr << std::endl
    << "vdmv_stat:" << regs->vdmv_stat << std::endl
  	<< "vdmv_ma_stat:" << regs->vdmv_ma_stat << std::endl
  	<< "vdmv_cp_error:" << regs->vdmv_cp_error << std::endl
  	<< "vdmv_delay_stat:" << regs->vdmv_delay_stat << std::endl
  	<< "vdsr1_stat:" << regs->vdsr1_stat << std::endl
  	<< "vdsr1_parse_stat:" << regs->vdsr1_parse_stat << std::endl
  	<< "vdsi_stat:" << regs->vdsi_stat << std::endl
  	<< "vdsi_error_stat:" << regs->vdsi_error_stat << std::endl
  	<< "vdsi_mod_stat:" << regs->vdsi_mod_stat << std::endl
    << "vdsi_fifo_error:" << regs->vdsi_fifo_error << std::endl
    << "vdsi_fifo_dma_stat:" << regs->vdsi_fifo_dma_stat << std::endl
  ;
}


struct device::device_impl {
  asio::posix::stream_descriptor fd;

  void* regs;

  void* buf;
  std::uint32_t buf_phys;

  std::uint32_t intmem_phys;
  std::uint32_t extmem_phys;
};

device::~device() {
  if(p) delete p;
}

register_map volatile* regs(device& d) {
  return reinterpret_cast<register_map*>(d.impl()->regs);
}

auto wait_for_interrupt = [](device d) {
  static std::uint32_t status;
  auto md = utils::move_on_copy(std::move(d));
  
  return utils::async_read_some(unwrap(md).impl()->fd, asio::mutable_buffers_1(&status, sizeof(status)))
  >> [=](std::size_t bytes) mutable {
    return std::make_tuple(std::move(unwrap(md)), status);
  };
};

static auto reset = [](device d) {
  regs(d)->vd_imsc = si_cmd_rdy;
  regs(d)->vdsi_cmd = si_cmd::suspend_bus_transfers;
  
  return 
    wait_for_interrupt(std::move(d))
    >> [](device d, std::uint32_t) {
      regs(d)->vd_imsc = mvde_cmd_rdy;
      regs(d)->vdmv_cmd = mvde_cmd::suspend_bus_transfers;
      
      return wait_for_interrupt(std::move(d));
    }
    >> [](device d, std::uint32_t status) {
      std::cout << "reseting si" << std::endl;

      regs(d)->vd_imsc = si_cmd_rdy;
      regs(d)->vdsi_cmd = si_cmd::reset;  
      
      return wait_for_interrupt(std::move(d));
    }
    >> [](device d, std::uint32_t status) {
      std::cout << "reseting mvde" << std::endl;

      regs(d)->vd_imsc = mvde_cmd_rdy;
      regs(d)->vdmv_cmd = mvde_cmd::reset;
      
      return wait_for_interrupt(std::move(d)); 
    }
    >> [](device d, std::uint32_t status) {
      regs(d)->vdmv_cmd = mvde_cmd::start_continuous_decoding;
      
      return d;
    };
};

void async_open(asio::io_service& io, std::function<void (std::error_code const&, device d)> completion) {
  asio::posix::stream_descriptor fd(io, ::open("/dev/msvdhd", O_RDWR));
  if(!fd.is_open()) throw std::system_error(errno, std::system_category());

  std::uint32_t* registers = reinterpret_cast<std::uint32_t*>(msvd_map_registers(fd.native_handle()));
  if(registers == MAP_FAILED) throw std::system_error(errno, std::system_category());
 
  void* buf = msvd_map_buffer(fd.native_handle());
  if(buf == MAP_FAILED) throw std::system_error(errno, std::system_category());

  unsigned long buf_phys = 0;
  if(msvd_get_buf_phys(fd.native_handle(), &buf_phys) <0) throw std::system_error(errno, std::system_category());
  
  unsigned long int_phys = 0;
  if(msvd_get_intmem_phys(fd.native_handle(), &int_phys) < 0) throw std::system_error(errno, std::system_category());

  unsigned long ext_phys = 0;
  if(msvd_get_extmem_phys(fd.native_handle(), &ext_phys) < 0) throw std::system_error(errno, std::system_category());

  device d{new device::device_impl{std::move(fd), registers, buf, buf_phys, int_phys, ext_phys}};

  regs(d)->vd_icr = 0xFFFFFFFF;

  msvd_enable_irq(d.impl()->fd.native_handle());

  reset(std::move(d)) += completion;//[](utils::expected<device, std::error_code>) {};
}

template<typename Int>
constexpr Int round_up(Int value, Int round) {
  return value % round ? value + round - (value % round) : value;
}

std::uint64_t* copy_swap(std::uint64_t const* first, std::uint64_t const* last, std::uint64_t* out) {
  while(first != last)
    *out++ = __builtin_bswap64(*first++);
  return out;
}

std::uint64_t* copy_swap(asio::const_buffer const* first, asio::const_buffer const* last, std::uint64_t* out) {
  alignas(std::uint64_t) std::uint8_t swap_buffer[4*1024];

  static_assert(sizeof(swap_buffer) % sizeof(std::uint64_t) == 0, "sizeof(swap_buffer) must be a multilple of sizeof(std::uint64_t)");

  auto used = 0;

  for(;first != last; ++first) {
    auto current_buffer = *first;
    
    while(buffer_size(current_buffer)) {
      auto n = std::min(sizeof(swap_buffer) - used, buffer_size(current_buffer));
      memcpy(swap_buffer + used, asio::buffer_cast<const void*>(current_buffer), n);

      used += n;
      current_buffer = current_buffer + n;

      if(used == sizeof(swap_buffer)) {
        out = copy_swap(reinterpret_cast<std::uint64_t const*>(swap_buffer), reinterpret_cast<std::uint64_t const*>(swap_buffer + used), out);
        used = 0;
      }
    }
  }

  while(used % 8 != 0) swap_buffer[used++] = 0;
  
  return copy_swap(reinterpret_cast<std::uint64_t const*>(swap_buffer), reinterpret_cast<std::uint64_t const*>(swap_buffer + used), out);
}

std::uint32_t su_nsuma_buffer(device& d, mpeg::sequence_header_t const& ph) { return d.impl()->intmem_phys; }

std::uint16_t hor_pic_size_in_mbs(mpeg::sequence_header_t const& sh) { return (sh.horizontal_size_value + 15) / 16; }
std::uint16_t ver_pic_size_in_mbs(mpeg::sequence_header_t const& sh) { return (sh.vertical_size_value + 15) / 16; }

inline std::uint32_t psuma_buffer_size(mpeg::sequence_header_t const& sh) {
  return round_up(hor_pic_size_in_mbs(sh)*ver_pic_size_in_mbs(sh)*128u, 1024u);
}

inline std::uint32_t psuma_buffer_top(device& d, mpeg::sequence_header_t const& sh) {
  return round_up(d.impl()->extmem_phys, 1024u);
}

inline std::uint32_t psuma_buffer_bot(device& d, mpeg::sequence_header_t const& sh) {
  return psuma_buffer_top(d,sh) + psuma_buffer_size(sh)/2;
}

using utils::operator "" _bf;

unsigned coding_type(mpeg::picture_coding pt) {
  using namespace mpeg;
  return pt == picture_coding::I ? 1 : (pt == picture_coding::P ? 2 : 3);
}

struct ct_hdr {
  std::uint32_t value;

  static constexpr auto ctxt_type =           0.2_bf;
  static constexpr auto load_table_sel =      2.8_bf;
  static constexpr auto table_scan =          10.5_bf;
  static constexpr auto last_mb_in_picture =  15.1_bf;
  static constexpr auto cp_mc_ip_en =         24.1_bf;
  static constexpr auto lf_en =               25.1_bf;
  static constexpr auto os_en =               26.1_bf;
  static constexpr auto cp_mc_ip_last_mb =    27.1_bf;
  static constexpr auto lf_last_mb =          28.1_bf;
  static constexpr auto os_last_mb =          29.1_bf;
  static constexpr auto cache_reset =         30.1_bf;
  static constexpr auto always_1 =            31.1_bf;     
};

enum {
  CT_INFO = 1,
  CT_WT = 2,
  CT_BA = 3
};

enum load_table_sel {
  w_intra_8x8 = 1, 
  w_inter_8x8 = 2
};

enum address_sel {
  frame_store_y_ba0 = 0,
  frame_store_c_ba0 = 1,
  frame_store_y_ba1 = 2,
  frame_store_c_ba1 = 3,
  frame_store_y_ba31 = frame_store_y_ba0 + 31*2,
  frame_store_c_ba31 = frame_store_y_ba0 + 31*2,
  cp_buf_ba = 0x40,
  ip_buf_ba = 0x41,
  os_buf_ba = 0x42,
  lf_buf_ba = 0x43,
  os_frame_store_y_ba = 0x44,
  os_frame_store_c_ba = 0x45,
  lf_frame_store_y_ba = 0x46,
  lf_frame_store_c_ba = 0x47
};

std::uint32_t ct_ba_element(std::uint32_t base, address_sel sel, bool last = 0) {
  return set(10.22_bf, base/(1 << 10)) | set(9.1_bf, last) | set(0.7_bf, sel);
}

struct ct_info {
  //word0
  static constexpr auto structure =     0.2_bf;
  static constexpr auto coding_type =   2.2_bf;
  static constexpr auto vert_pic_size = 4.8_bf;
  static constexpr auto hor_pic_size =  12.8_bf;

  //word1
  static constexpr auto pred_mode =     0.2_bf;
  static constexpr auto iq_wt_en =      4.1_bf;
  static constexpr auto iq_sat_en =     6.1_bf;
  static constexpr auto iq_div3 =       8.3_bf;
  static constexpr auto iq_div_ctrl =   11.1_bf;
  static constexpr auto def_dc_ctrl =   12.2_bf;
  static constexpr auto iq_table_mask = 14.4_bf;
  static constexpr auto mc_mode =       18.3_bf;
  static constexpr auto scan_mode =     22.6_bf;

  //word6
  static constexpr auto lf_mode =       13.3_bf; 
  static constexpr auto os_mode =       16.3_bf;
  static constexpr auto hor_fbuf_size = 19.8_bf;
};

struct mpeg_si_info {
  // word0
  static constexpr auto decoding_mode           = 2.3_bf;
  static constexpr auto progressive             = 5.1_bf;
  static constexpr auto mb_qp_luma              = 8.6_bf;
  static constexpr auto f_code00                = 14.4_bf;
  static constexpr auto f_code01                = 18.4_bf;
  static constexpr auto f_code10                = 22.4_bf;
  static constexpr auto f_code11                = 26.4_bf;
  static constexpr auto concealment_vectors     = 30.1_bf;
  static constexpr auto frame_pred_frame_dct    = 31.1_bf;
    
  // word1
  static constexpr auto intra_vlc_format        = 16.1_bf;

  // word2
  static constexpr auto full_pel_forward_flag   = 5.1_bf;
  static constexpr auto full_pel_backward_flag  = 6.1_bf;
  static constexpr auto q_scale_type            = 16.1_bf;
  static constexpr auto top_field_first         = 17.1_bf;
};

std::uint32_t volatile* dma_element(std::uint32_t volatile* cfg, device& d, utils::range<std::uint32_t volatile*> range, bool last = false) {
  *cfg++ = set(30.2_bf, 0) | set(16.14_bf, range.size()) | set(15.1_bf, last) | set(2.12_bf, begin(range) - regs(d)->vdsi_cfg_data);
  return cfg;
}

std::uint32_t volatile* dma_element_coeff(std::uint32_t volatile* cfg, device& d, utils::range<std::uint32_t volatile*> range, bool last = false) {
  *cfg++ = set(30.2_bf, 2) | set(16.14_bf, range.size()) | set(15.1_bf, last) | set(2.12_bf, begin(range) - regs(d)->vdsi_cfg_data);
  return cfg;
}

std::uint32_t volatile* dma_element(std::uint32_t volatile* cfg, device& d, std::uint32_t volatile* reg, utils::range<std::uint32_t volatile*> range, bool last = false) {
  *cfg++ = set(30.2_bf, 3) | set(16.14_bf, range.size()) | set(15.1_bf, last) | set(2.12_bf, begin(range) - regs(d)->vdsi_cfg_data);
  *cfg++ = (reg - &regs(d)->vdsr1_cmd)*sizeof(std::uint32_t);
  return cfg;
}

struct dma_list {
  static constexpr auto ad = 0.14_bf;
  static constexpr auto en = 31.1_bf;
};

void config_sr1(device& d, std::uint8_t* start, int offset, std::uint8_t* end) {
  auto r = regs(d);
 
  r->vdsr1_search_range   = 0x100000;
  r->vdsr1_search_pattern_1 = 0x00000100;
  r->vdsr1_search_mask_1  = 0xFFFFFF00;
  r->vdsr1_cfg            = 0x07;
  r->vdsr1_str_buf_ba     = d.impl()->buf_phys;
  r->vdsr1_str_buf_size   = 2*1024*1024;
  r->vdsr1_str_buf_blen   = 1024*1024;
  r->vdsr1_str_buf_thresh = 0x000fffc0;
  r->vdsr1_str_offs       = (start - reinterpret_cast<std::uint8_t*>(d.impl()->buf)) * 8 + offset;
  r->vdsr1_str_offs_end   = (end - reinterpret_cast<std::uint8_t*>(d.impl()->buf)) * 8;

  __sync_synchronize();

  r->vdsr1_cmd = 6; // continue;

  //dump_sr1_regs(std::cout, r);
}

static auto start_decode = [](device d) {
  regs(d)->vd_imsc = mvde_proc_err | mvde_last_mb_rdy | si_parse_err | sr1_si_access_failed; 
  regs(d)->vdsi_cmd = si_cmd::decode_until_return_point;

  auto timer = std::make_shared<asio::system_timer>(d.impl()->fd.get_io_service());
  timer->expires_from_now(std::chrono::seconds(5));

  auto r = regs(d);

  timer->async_wait([=](std::error_code const& ec) {
    if(!ec) {
      dump_stats_regs(std::cout, r); 
      dump_sr1_regs(std::cout, r); 
    }
  });

  return 
    wait_for_interrupt(std::move(d))
    >> 
    [timer](device d, std::uint32_t status) {
      timer->cancel();
      if(!(status & mvde_last_mb_rdy)) {
        dump_stats_regs(std::cout, regs(d)); 
        dump_sr1_regs(std::cout, regs(d));
        throw std::system_error(make_error_code(errc::unspecified_failure));
      }
      return d; 
    }; 
};

void configure(device& d,
  mpeg::sequence_header_t const& sh,
  mpeg::picture_header_t const& ph,
  mpeg::picture_coding_extension_t const* pcx,
  buffer_geometry const& geometry,
  std::uint32_t curpic,
  std::uint32_t refpic1,
  std::uint32_t refpic2)
{
  std::uint32_t volatile* cfg = regs(d)->vdsi_cfg_data, *start = cfg;

//  std::cout << "cfg:" << std::hex << ((std::uint8_t*)cfg - (std::uint8_t*)regs(d)) << std::endl;
//  std::cout << geometry << std::endl;
//  std::cout << ph << std::endl;
//  if(pcx) std::cout << *pcx << std::endl;
 
  *cfg++ = set(ct_hdr::always_1, 1) | set(ct_hdr::ctxt_type, CT_BA);
  
  if(ph.picture_coding_type == mpeg::picture_coding::B) std::swap(refpic1, refpic2);

  if(refpic1) {
    *cfg++ = ct_ba_element(refpic1 + geometry.luma_offset, frame_store_y_ba0);
    *cfg++ = ct_ba_element(refpic1 + geometry.chroma_offset, frame_store_c_ba0);

    if(refpic2) {
      *cfg++ = ct_ba_element(refpic2 + geometry.luma_offset, frame_store_y_ba1);
      *cfg++ = ct_ba_element(refpic2 + geometry.chroma_offset, frame_store_c_ba1);
    }
  }

  *cfg++ = ct_ba_element(curpic + geometry.luma_offset, lf_frame_store_y_ba);
  *cfg++ = ct_ba_element(curpic + geometry.chroma_offset, lf_frame_store_c_ba, true);

  auto ct_ba = utils::make_range(start, cfg);

  start = cfg;
  *cfg++ = set(ct_hdr::always_1, 1) | set(ct_hdr::ctxt_type, CT_INFO);
  *cfg++ = 0
    | set(ct_info::structure,     pcx ? pcx->picture_structure : 3)
    | set(ct_info::coding_type,   coding_type(ph.picture_coding_type))
    | set(ct_info::vert_pic_size, ver_pic_size_in_mbs(sh))
    | set(ct_info::hor_pic_size,  hor_pic_size_in_mbs(sh));
  *cfg++ = 0
    | set(ct_info::pred_mode,     1)
    | set(ct_info::iq_wt_en,      1)
    | set(ct_info::iq_sat_en,     1)
    | set(ct_info::iq_div3,       pcx ? 5 : 4)
    | set(ct_info::iq_div_ctrl,   1)
    | set(ct_info::def_dc_ctrl,   pcx ? pcx->intra_dc_precision : 0)
    | set(ct_info::iq_table_mask, 0xE)
    | set(ct_info::mc_mode,       pcx ? 1 : 0)
    | set(ct_info::scan_mode,     pcx && pcx->alternate_scan ? 3 : 2);
  *cfg++ = 0;
  *cfg++ = 0;
  *cfg++ = 0;
  *cfg++ = 0;
  *cfg++ = 0 
    | set(ct_info::lf_mode, 3)
    | set(ct_info::os_mode, 3)
    | set(ct_info::hor_fbuf_size, geometry.width / 16);

  auto ct_info = utils::make_range(start, cfg);

  start = cfg; // ct_wt8 
  *cfg++ = 0
    | set(ct_hdr::always_1, 1)
    | set(ct_hdr::table_scan, pcx && pcx->alternate_scan ? 14 : 13) 
    | set(ct_hdr::load_table_sel, w_intra_8x8 | w_inter_8x8) 
    | set(ct_hdr::ctxt_type, CT_WT);

  for(int i = 0; i != 32; ++i)
    *cfg++ = sh.intra_quantiser_matrix[i*2] << 8 | sh.intra_quantiser_matrix[i*2+1] | (i == 31) << 30;

  for(int i = 0; i != 32; ++i)
    *cfg++ = sh.non_intra_quantiser_matrix[i*2] << 8 | sh.non_intra_quantiser_matrix[i*2+1] | (i == 31) << 30;
  auto ct_wt = utils::make_range(start, cfg);
 
  start = cfg; //si_info
  *cfg++ = 0
    | set(mpeg_si_info::decoding_mode,  pcx ? 2 : 1)
    | set(mpeg_si_info::progressive,    pcx && pcx->progressive_frame)
    | set(mpeg_si_info::mb_qp_luma,     pcx ? (1 << (3 - pcx->intra_dc_precision)) : 8)
    | set(mpeg_si_info::f_code00,       pcx ? pcx->f_code[0][0] : ph.forward_f_code)
    | set(mpeg_si_info::f_code01,       pcx ? pcx->f_code[0][1] : ph.forward_f_code)
    | set(mpeg_si_info::f_code10,       pcx ? pcx->f_code[1][0] : ph.backward_f_code)
    | set(mpeg_si_info::f_code11,       pcx ? pcx->f_code[1][1] : ph.backward_f_code)
    | set(mpeg_si_info::concealment_vectors, pcx && pcx->concealment_motion_vectors)
    | set(mpeg_si_info::frame_pred_frame_dct, pcx && pcx->frame_pred_frame_dct);

  *cfg++ = set(mpeg_si_info::intra_vlc_format, pcx && pcx->intra_vlc_format);
  *cfg++ = 0
    | set(mpeg_si_info::full_pel_forward_flag,  ph.full_pel_forward_vector)
    | set(mpeg_si_info::full_pel_backward_flag, ph.full_pel_backward_vector)
    | set(mpeg_si_info::q_scale_type,           pcx && pcx->q_scale_type)
    | set(mpeg_si_info::top_field_first,        pcx && pcx->top_field_first);
  *cfg++ = 0;
  auto si_info = utils::make_range(start, cfg);

  start = cfg; //si_ba
  *cfg++ = su_nsuma_buffer(d, sh);
  *cfg++ = psuma_buffer_top(d, sh);
  *cfg++ = psuma_buffer_bot(d, sh);
  *cfg++ = psuma_buffer_top(d, sh);
  *cfg++ = psuma_buffer_bot(d, sh);
  auto si_ba = utils::make_range(start, cfg);

  auto vdsi_cfg = cfg++;
  *vdsi_cfg = 0x80;

  auto vdsi_conceal_cfg = cfg++;
  *vdsi_conceal_cfg = 1 << 17;

  start = cfg;
  cfg = dma_element(cfg, d, &regs(d)->vdsi_param_1, si_info);
  cfg = dma_element(cfg, d, &regs(d)->vdsi_param_6, utils::make_range(begin(ct_info)+1, end(ct_info)));
  cfg = dma_element(cfg, d, &regs(d)->vdsi_cfg, utils::make_range(vdsi_cfg, vdsi_cfg+1));
  cfg = dma_element(cfg, d, &regs(d)->vdsi_conceal_cfg, utils::make_range(vdsi_conceal_cfg, vdsi_conceal_cfg+1));
  cfg = dma_element(cfg, d, &regs(d)->vdsi_su_nsuma_ba, si_ba, true);
  regs(d)->vdsi_dma_list1_ad = set(dma_list::en, 1) | set(dma_list::ad, sizeof(std::uint32_t)*(start - regs(d)->vdsi_cfg_data));

  start = cfg;
  cfg = dma_element(cfg, d, ct_ba);
  cfg = dma_element(cfg, d, utils::make_range(begin(ct_wt), begin(ct_wt)+1));
  cfg = dma_element_coeff(cfg, d, utils::make_range(begin(ct_wt)+1, end(ct_wt)));
  cfg = dma_element(cfg, d, ct_info, true);
  regs(d)->vdsi_dma_list0_ad = set(dma_list::en, 1) | set(dma_list::ad, sizeof(std::uint32_t)*(start - regs(d)->vdsi_cfg_data));
}

void async_decode(
  device d,
  mpeg::sequence_header_t const& sh,
  mpeg::picture_header_t const& ph,
  mpeg::picture_coding_extension_t const* pcx,
  buffer_geometry const& geometry,
  std::uint32_t curpic,
  std::uint32_t refpic1,
  std::uint32_t refpic2,
  utils::range<asio::const_buffer const*> picture_data,
  std::function<void (std::error_code const& ec, device)> complete)
{
  configure(d, sh, ph, pcx, geometry, curpic, refpic1, refpic2);

  auto buf_end = copy_swap(begin(picture_data), end(picture_data), reinterpret_cast<std::uint64_t*>(d.impl()->buf));
  *buf_end++ = 0x0000010000000000ul;

  config_sr1(d, reinterpret_cast<std::uint8_t*>(d.impl()->buf), 0, reinterpret_cast<std::uint8_t*>(buf_end));
  
  start_decode(std::move(d)) += std::move(complete);
}

} //msvd


