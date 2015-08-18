
individual_metrics = [ 'req_trig_update', 'local_update', 'remote_update', 'update_cost', 'req_trig_m_inv', 'local_m_inv', 'remote_m_inv', 'm_inv_cost', 'broadcast', 'total_update', 'total_m_inv' ]

grouped_metrics = [ 'update_cost', 'm_inv_cost' ]

stacked_metrics = [ 'nonwrite_broadcast', 'write_broadcast', 'local_m_inv', 'remote_m_inv', 'local_update', 'remote_update' ]

m_prot_name = {}
m_prot_name['dhccp'] = "DHCCP"
m_prot_name['rwt'] = "RWT"
m_prot_name['hmesi'] = "HMESI"

m_app_name = {}
m_app_name['mandel'] = "Mandelbrot"
m_app_name['filter'] = "Filter"
m_app_name['filt_ga'] = "Filter (opt.)"
m_app_name['histogram'] = "Histogram"
m_app_name['kmeans'] = "Kmeans"
m_app_name['pca'] = "PCA"
m_app_name['mat_mult'] = "Matrix Mult."
m_app_name['barnes'] = "Barnes"
m_app_name['fmm'] = "FMM"
m_app_name['ocean'] = "Ocean"
m_app_name['raytrace'] = "Raytrace"
m_app_name['radiosity'] = "Radiosity"
m_app_name['waters'] = "Water Sp."
m_app_name['watern'] = "Water Nsq."
m_app_name['cholesky'] = "Cholesky"
m_app_name['lu'] = "LU"
m_app_name['fft'] = "FFT"
m_app_name['radix'] = "Radix"
m_app_name['fft_ga'] = "FFT (opt.)"
m_app_name['radix_ga'] = "Radix (opt.)"

m_metric_id = {}
m_metric_tag = {}
m_metric_tag['counter_reset']      = "[000]"
m_metric_tag['ncycles']            = "[001]"

m_metric_tag['local_read']         = "[010]"
m_metric_tag['remote_read']        = "[011]"
m_metric_tag['read_cost']          = "[012]"

m_metric_tag['local_write']        = "[020]"
m_metric_tag['remote_write']       = "[021]"
m_metric_tag['write_flits_local']  = "[022]"
m_metric_tag['write_flits_remote'] = "[023]"
m_metric_tag['write_cost']         = "[024]"
m_metric_tag['write_l1_miss_ncc']  = "[025]"

m_metric_tag['local_ll']           = "[030]"
m_metric_tag['remote_ll']          = "[031]"
m_metric_tag['ll_cost']            = "[032]"

m_metric_tag['local_sc']           = "[040]"
m_metric_tag['remote_sc']          = "[041]"
m_metric_tag['sc_cost']            = "[042]"

m_metric_tag['local_cas']          = "[050]"
m_metric_tag['remote_cas']         = "[051]"
m_metric_tag['cas_cost']           = "[052]"

m_metric_tag['req_trig_update']    = "[060]"
m_metric_tag['local_update']       = "[061]"
m_metric_tag['remote_update']      = "[062]"
m_metric_tag['update_cost']        = "[063]"

m_metric_tag['req_trig_m_inv']     = "[070]"
m_metric_tag['local_m_inv']        = "[071]"
m_metric_tag['remote_m_inv']       = "[072]"
m_metric_tag['m_inv_cost']         = "[073]"

m_metric_tag['broadcast']          = "[080]"
m_metric_tag['write_broadcast']    = "[081]"
m_metric_tag['getm_broadcast']     = "[082]"

m_metric_tag['local_cleanup']      = "[090]"
m_metric_tag['remote_cleanup']     = "[091]"
m_metric_tag['cleanup_cost']       = "[092]"
m_metric_tag['local_cleanup_d']    = "[093]"
m_metric_tag['remote_cleanup_d']   = "[094]"
m_metric_tag['cleanup_d_cost']     = "[095]"

m_metric_tag['read_miss']          = "[100]"
m_metric_tag['write_miss']         = "[101]"
m_metric_tag['write_dirty']        = "[102]"
m_metric_tag['getm_miss']          = "[103]"

m_metric_tag['read_hit_trt']       = "[110]" # Reads blocked by a hit in the TRT
m_metric_tag['trans_full_trt']     = "[111]" # Transactions blocked because the TRT is full
m_metric_tag['put']                = "[120]"
m_metric_tag['get']                = "[121]"
m_metric_tag['min_heap_slots_av']  = "[130]"

m_metric_tag['ncc_to_cc_read']     = "[140]"
m_metric_tag['ncc_to_cc_write']    = "[141]"

m_metric_tag['local_getm']         = "[150]"
m_metric_tag['remote_getm']        = "[151]"
m_metric_tag['getm_cost']          = "[152]"

m_metric_tag['local_inval_ro']     = "[160]"
m_metric_tag['remote_inval_ro']    = "[161]"
m_metric_tag['inval_ro_cost']      = "[162]"



all_metrics = m_metric_tag.keys()
all_tags = m_metric_tag.values()

m_metric_name = {}
m_metric_name['counter_reset']       = "Counters reset at cycle"
m_metric_name['ncycles']             = "Number of Cycles"

m_metric_name['local_read']          = "Number of Local Reads (Miss in L1)"
m_metric_name['remote_read']         = "Number of Remote Reads (Miss in L1)"
m_metric_name['read_cost']           = "Read Cost"

m_metric_name['local_write']         = "Number of Local Writes"
m_metric_name['remote_write']        = "Number of Remote Writes"
m_metric_name['write_flits_local']   = "Number of Local Write Flits"
m_metric_name['write_flits_remote']  = "Number of Remote Write Flits"
m_metric_name['write_cost']          = "Write Cost"

m_metric_name['local_ll']            = "Number of Local LL"
m_metric_name['remote_ll']           = "Number of Remote LL"
m_metric_name['ll_cost']             = "LL Cost"

m_metric_name['local_sc']            = "Number of Local SC"
m_metric_name['remote_sc']           = "Number of Remote SC"
m_metric_name['sc_cost']             = "SC Cost"

m_metric_name['local_cas']           = "Number of Local CAS"
m_metric_name['remote_cas']          = "Number of Remote CAS"
m_metric_name['cas_cost']            = "CAS Cost"

m_metric_name['req_trig_update']     = "Number of Requests Triggering an Update"
m_metric_name['local_update']        = "Number of Local Updates"
m_metric_name['remote_update']       = "Number of Remote Updates"
m_metric_name['update_cost']         = "Update Cost"

m_metric_name['req_trig_m_inv']      = "Number of Requests Triggering a M.inv"
m_metric_name['local_m_inv']         = "Number of Local Multi Inval"
m_metric_name['remote_m_inv']        = "Number of Remote Multi Inval"
m_metric_name['m_inv_cost']          = "Multi Inval Cost"

m_metric_name['broadcast']           = "Total Number of Broadcasts"
m_metric_name['write_broadcast']     = "Number of Broadcasts Trig. by Writes"
m_metric_name['nonwrite_broadcast']  = "Number of Broadcasts not Trig. by Writes"
m_metric_name['getm_broadcast']      = "Number of Broadcasts Trig. by GetM"

m_metric_name['local_cleanup']       = "Number of Local Cleanups"
m_metric_name['remote_cleanup']      = "Number of Remote Cleanups"
m_metric_name['cleanup_cost']        = "Cleanup Cost"
m_metric_name['local_cleanup_d']     = "Number of Local Cleanups with Data"
m_metric_name['remote_cleanup_d']    = "Number of Remote Cleanups with Data"
m_metric_name['cleanup_d_cost']      = "Cleanup with Data Cost"

m_metric_name['read_miss']           = "Number of Read Miss (in L2)"
m_metric_name['write_miss']          = "Number of Write Miss (in L2)"
m_metric_name['write_dirty']         = "Number of Write Dirty (from L2 to Memory)"
m_metric_name['getm_miss']           = "Number of GetM Miss (in L2)"

m_metric_name['read_hit_trt']        = "Number of Reads Blocked by a Hit in TRT" # Reads blocked by a hit in the TRT
m_metric_name['trans_full_trt']      = "Number of Transactions Blocked because the TRT is Full" # Transactions blocked because the TRT is full
m_metric_name['put']                 = "Number of PUT to Memory"
m_metric_name['get']                 = "Number of GET from Memory"
m_metric_name['min_heap_slots_av']   = "Minimum Number of Heap Slots available"

m_metric_name['ncc_to_cc_read']      = "Number or Reads trig. NCC to CC"
m_metric_name['ncc_to_cc_write']     = "Number of Writes trig. NCC to CC"

m_metric_name['local_getm']          = "Number of Local GetM"
m_metric_name['remote_getm']         = "Number of Remote GetM"
m_metric_name['getm_cost']           = "GetM Cost"

m_metric_name['local_inval_ro']      = "Number of Local Inval RO"
m_metric_name['remote_inval_ro']     = "Number of Remote Inval RO"
m_metric_name['inval_ro_cost']       = "Inval RO Cost"


m_metric_name['total_read']          = "Total Number of Reads"
m_metric_name['total_write']         = "Total Number of Writes"
m_metric_name['total_ll']            = "Total Number of LL"
m_metric_name['total_sc']            = "Total Number of SC"
m_metric_name['total_cas']           = "Total Number of CAS"
m_metric_name['total_update']        = "Total Number of Updates"
m_metric_name['total_m_inv']         = "Total Number of Multi Inval"
m_metric_name['total_cleanup']       = "Total Number of Cleanups"
m_metric_name['total_cleanup_d']     = "Total Number of Cleanups with Data"
m_metric_name['total_direct']        = "Total Number of Direct Requests"
m_metric_name['total_ncc_to_cc']     = "Total Number of NCC to CC Changes"
m_metric_name['broadcast_cost']      = "Broadcast Cost"
m_metric_name['direct_cost']         = "Direct Requests Cost"
m_metric_name['total_stacked']       = "??" # Normalization factor, does not have a name (unused)



m_metric_norm = {} # "N" (None), P (#procs), C (#cycles), W (#writes), R (#reads), D (#direct req -- R ou W), n (value with n proc(s))
m_metric_norm['req_trig_update']   = "C"
m_metric_norm['local_update']      = "C"
m_metric_norm['remote_update']     = "C"
m_metric_norm['update_cost']       = "C"
m_metric_norm['req_trig_m_inv']    = "C"
m_metric_norm['local_m_inv']       = "C"
m_metric_norm['remote_m_inv']      = "C"
m_metric_norm['m_inv_cost']        = "C"
m_metric_norm['broadcast']         = "C"
m_metric_norm['total_update']      = "C"
m_metric_norm['total_m_inv']       = "C"

m_norm_factor_name = {}
m_norm_factor_name['N']   = ""
m_norm_factor_name['P']   = "Normalized w.r.t.\\nthe Number of Processors"
m_norm_factor_name['C']   = "Normalized w.r.t.\\nthe Number of Cycles (x 1000)"
m_norm_factor_name['W']   = "Normalized w.r.t.\\nthe Number of Writes"
m_norm_factor_name['R']   = "Normalized w.r.t.\\nthe Number of Reads"
m_norm_factor_name['D']   = "Normalized w.r.t.\\nthe Number of Direct Requests"
m_norm_factor_name['1']   = "Normalized w.r.t.\\nthe Value on 1 Processor"
m_norm_factor_name['4']   = "Normalized w.r.t.\\nthe Value on 4 Processors"
m_norm_factor_name['8']   = "Normalized w.r.t.\\nthe Value on 8 Processors"
m_norm_factor_name['16']  = "Normalized w.r.t.\\nthe Value on 16 Processors"
m_norm_factor_name['32']  = "Normalized w.r.t.\\nthe Value on 32 Processors"
m_norm_factor_name['64']  = "Normalized w.r.t.\\nthe Value on 64 Processors"
m_norm_factor_name['128'] = "Normalized w.r.t.\\nthe Value on 128 Processors"
m_norm_factor_name['256'] = "Normalized w.r.t.\\nthe Value on 256 Processors"



colors = [ "\"#008000\"", "\"#000080\"", "\"#BADC98\"", "\"#BA98DC\"", "\"#98DCBA\"", "\"#98BADC\"", "\"#BA9876\"", "\"#BA7698\"", "\"#98BA76\"", "\"#9876BA\"", "\"#76BA98\"", "\"#7698BA\"" ]


