[graphify] MultiDiGraph edge-collapse diagnostic
input: <in-memory>
input_stage: provided JSON (normal graph.json is post-build)
effective_directed: <direct-call>
nodes: 5608
unverified_code_nodes: 0
raw_edges: 12159
valid_candidate_edges: 11544
missing_endpoint_edges: 0
dangling_endpoint_edges: 613
external_reference_edges: 2
self_loop_edges: 0
exact_duplicate_edges: 0
directed_unique_endpoint_pairs: 11410
directed_same_endpoint_collapsed_edges: 134
undirected_unique_endpoint_pairs: 11366
undirected_same_endpoint_collapsed_edges: 178
same_endpoint_group_count: 132
relation_variant_groups: 17
source_file_variant_groups: 0
source_location_variant_groups: 15
context_variant_groups: 115
post_build_graph_type: Graph
post_build_edges: 11979
producer_suppression_sites: 12
producer_suppression_examples:
  - L1749 seen_ids arity=unknown
  - L2282 seen_ids arity=unknown
  - L2284 seen_doc_refs arity=unknown
  - L2762 seen_ids arity=unknown
  - L2909 seen_ids arity=unknown
  - L3743 seen_keys arity=unknown
  - L3916 seen_keys arity=unknown
  - L6273 seen_ids arity=unknown
examples:
  - src_mapping_uniquename -> src_mapping_cpp_string edges=3 relations=['references'] locations=['L1120'] contexts=['generic_arg', 'parameter_type', 'return_type']
  - src_settings_resolvefontpath -> src_settings_cpp_string edges=3 relations=['references'] locations=['L48'] contexts=['generic_arg', 'parameter_type', 'return_type']
  - engine_core_calib_lidarplacement_lidartosurface -> engine_core_math_vec2_vec2 edges=2 relations=['references'] locations=['L17'] contexts=['parameter_type', 'return_type']
  - engine_core_calib_lidarplacement_surfacetolidar -> engine_core_math_vec2_vec2 edges=2 relations=['references'] locations=['L22'] contexts=['parameter_type', 'return_type']
  - engine_core_filter_oneeurofilter_oneeurofilter2d_filter -> engine_core_math_vec2_vec2 edges=2 relations=['references'] locations=['L96'] contexts=['parameter_type', 'return_type']
note: normal graph.json is post-build; raw producer loss must be measured earlier.