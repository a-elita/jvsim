import lsp_stats_pb2

def jv_encode_lsp_stats(jnpr, index, lc_slot, system_params):
    lsp_stats = jnpr.Extensions[lsp_stats_pb2.jnpr_lsp_statistics_ext]
    record    = lsp_stats.lsp_stats_records.add()
    record.name = "foo-" + str(index)
    record.instance_identifier = 100
    record.counter_name = "counter1"
    record    = lsp_stats.lsp_stats_records.add()
    record.name = "foo000-" + str(index)
    record.instance_identifier = 0
    record.counter_name = "counter1"
