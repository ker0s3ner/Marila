if(NOT DEFINED MARILA_EXECUTABLE OR NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "MARILA_EXECUTABLE and SOURCE_DIR are required")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" --version
    RESULT_VARIABLE VERSION_RESULT
    OUTPUT_VARIABLE VERSION_OUTPUT
)
if(VERSION_RESULT OR NOT VERSION_OUTPUT MATCHES "Marila native 0.1.0")
    message(FATAL_ERROR "version command failed: ${VERSION_OUTPUT}")
endif()

set(AST_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-ast.json")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" ast "${SOURCE_DIR}/examples/higher_order_demo.marila"
    RESULT_VARIABLE AST_RESULT
    OUTPUT_FILE "${AST_OUTPUT}"
    ERROR_VARIABLE AST_ERROR
)
if(AST_RESULT)
    message(FATAL_ERROR "ast command failed: ${AST_ERROR}")
endif()
file(READ "${AST_OUTPUT}" AST_TEXT)
if(NOT AST_TEXT MATCHES "\"kind\":\"program\"")
    message(FATAL_ERROR "ast output did not identify a program")
endif()
if(NOT AST_TEXT MATCHES "\"kind\":\"lambda\"")
    message(FATAL_ERROR "ast output did not preserve lambda expressions")
endif()

set(TYPED_AST_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-typed-ast.json")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" ast "${SOURCE_DIR}/examples/typed_function_demo.marila"
    RESULT_VARIABLE TYPED_AST_RESULT
    OUTPUT_FILE "${TYPED_AST_OUTPUT}"
    ERROR_VARIABLE TYPED_AST_ERROR
)
if(TYPED_AST_RESULT)
    message(FATAL_ERROR "typed ast command failed: ${TYPED_AST_ERROR}")
endif()
file(READ "${TYPED_AST_OUTPUT}" TYPED_AST_TEXT)
if(NOT TYPED_AST_TEXT MATCHES "\"parameter_types\"" OR NOT TYPED_AST_TEXT MATCHES "\"type\":\"number\"")
    message(FATAL_ERROR "typed AST did not preserve function annotations")
endif()

set(TOKENS_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-tokens.json")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" tokens "${SOURCE_DIR}/examples/higher_order_demo.marila"
    RESULT_VARIABLE TOKENS_RESULT
    OUTPUT_FILE "${TOKENS_OUTPUT}"
    ERROR_VARIABLE TOKENS_ERROR
)
if(TOKENS_RESULT)
    message(FATAL_ERROR "tokens command failed: ${TOKENS_ERROR}")
endif()
file(READ "${TOKENS_OUTPUT}" TOKENS_TEXT)
if(NOT TOKENS_TEXT MATCHES "\"kind\":\"tokens\"" OR NOT TOKENS_TEXT MATCHES "\"kind\":\"identifier\"" OR NOT TOKENS_TEXT MATCHES "\"line\":1")
    message(FATAL_ERROR "token output did not contain token kinds and source positions")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/invalid_vector_type.marila"
    RESULT_VARIABLE INVALID_VECTOR_TYPE_RESULT
    ERROR_VARIABLE INVALID_VECTOR_TYPE_ERROR
)
if(NOT INVALID_VECTOR_TYPE_RESULT OR NOT INVALID_VECTOR_TYPE_ERROR MATCHES "not of type vector")
    message(FATAL_ERROR "invalid vector annotation was accepted: ${INVALID_VECTOR_TYPE_ERROR}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" check "${SOURCE_DIR}/examples/invalid_duplicate_point.marila"
    RESULT_VARIABLE INVALID_DUPLICATE_POINT_RESULT
    ERROR_VARIABLE INVALID_DUPLICATE_POINT_ERROR
)
if(NOT INVALID_DUPLICATE_POINT_RESULT OR NOT INVALID_DUPLICATE_POINT_ERROR MATCHES "duplicate point 'A'")
    message(FATAL_ERROR "duplicate visual point was accepted: ${INVALID_DUPLICATE_POINT_ERROR}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/invalid_typed_reassignment.marila"
    RESULT_VARIABLE INVALID_TYPED_ASSIGNMENT_RESULT
    ERROR_VARIABLE INVALID_TYPED_ASSIGNMENT_ERROR
)
if(NOT INVALID_TYPED_ASSIGNMENT_RESULT OR NOT INVALID_TYPED_ASSIGNMENT_ERROR MATCHES "value for 'values' is not of type vector")
    message(FATAL_ERROR "typed reassignment was accepted: ${INVALID_TYPED_ASSIGNMENT_ERROR}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/invalid_typed_index_assignment.marila"
    RESULT_VARIABLE INVALID_TYPED_INDEX_RESULT
    ERROR_VARIABLE INVALID_TYPED_INDEX_ERROR
)
if(NOT INVALID_TYPED_INDEX_RESULT OR NOT INVALID_TYPED_INDEX_ERROR MATCHES "indexed assignment would violate type vector")
    message(FATAL_ERROR "typed indexed assignment was accepted: ${INVALID_TYPED_INDEX_ERROR}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/invalid_imported_type.marila"
    RESULT_VARIABLE INVALID_IMPORTED_TYPE_RESULT
    ERROR_VARIABLE INVALID_IMPORTED_TYPE_ERROR
)
if(NOT INVALID_IMPORTED_TYPE_RESULT OR NOT INVALID_IMPORTED_TYPE_ERROR MATCHES "value for 'values' is not of type vector")
    message(FATAL_ERROR "imported type metadata was lost: ${INVALID_IMPORTED_TYPE_ERROR}")
endif()

set(JSON_EXPORT_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-export.json")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" export "${SOURCE_DIR}/examples/hybrid_program.marila" json "${JSON_EXPORT_OUTPUT}"
    RESULT_VARIABLE JSON_EXPORT_RESULT
    ERROR_VARIABLE JSON_EXPORT_ERROR
)
if(JSON_EXPORT_RESULT)
    message(FATAL_ERROR "JSON export failed: ${JSON_EXPORT_ERROR}")
endif()
file(READ "${JSON_EXPORT_OUTPUT}" JSON_EXPORT_TEXT)
if(NOT JSON_EXPORT_TEXT MATCHES "\"kind\":\"program\"" OR NOT JSON_EXPORT_TEXT MATCHES "\"statements\":")
    message(FATAL_ERROR "JSON export did not contain the program AST")
endif()

set(SHEET_JSON_EXPORT_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-sheet.json")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" export "${SOURCE_DIR}/examples/sheet_demo.marila" json "${SHEET_JSON_EXPORT_OUTPUT}"
    RESULT_VARIABLE SHEET_JSON_EXPORT_RESULT
    ERROR_VARIABLE SHEET_JSON_EXPORT_ERROR
)
if(SHEET_JSON_EXPORT_RESULT)
    message(FATAL_ERROR "sheet JSON export failed: ${SHEET_JSON_EXPORT_ERROR}")
endif()
file(READ "${SHEET_JSON_EXPORT_OUTPUT}" SHEET_JSON_EXPORT_TEXT)
if(NOT SHEET_JSON_EXPORT_TEXT MATCHES "\"kind\":\"sheet\"" OR NOT SHEET_JSON_EXPORT_TEXT MATCHES "\"reference\":\"C1\"" OR NOT SHEET_JSON_EXPORT_TEXT MATCHES "\"type\":\"bar\"")
    message(FATAL_ERROR "sheet JSON export was incomplete: ${SHEET_JSON_EXPORT_TEXT}")
endif()
set(SHEET_CSV_EXPORT_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-sheet.csv")
set(SHEET_TSV_EXPORT_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-sheet.tsv")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" export "${SOURCE_DIR}/examples/sheet_demo.marila" csv "${SHEET_CSV_EXPORT_OUTPUT}"
    RESULT_VARIABLE SHEET_CSV_RESULT
    ERROR_VARIABLE SHEET_CSV_ERROR
)
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" export "${SOURCE_DIR}/examples/sheet_demo.marila" tsv "${SHEET_TSV_EXPORT_OUTPUT}"
    RESULT_VARIABLE SHEET_TSV_RESULT
    ERROR_VARIABLE SHEET_TSV_ERROR
)
if(SHEET_CSV_RESULT OR SHEET_TSV_RESULT)
    message(FATAL_ERROR "sheet delimited export failed: ${SHEET_CSV_ERROR} ${SHEET_TSV_ERROR}")
endif()
file(READ "${SHEET_CSV_EXPORT_OUTPUT}" SHEET_CSV_TEXT)
file(READ "${SHEET_TSV_EXPORT_OUTPUT}" SHEET_TSV_TEXT)
if(NOT SHEET_CSV_TEXT MATCHES "A1" OR NOT SHEET_CSV_TEXT MATCHES "A1 \\+ B1" OR NOT SHEET_TSV_TEXT MATCHES "A1")
    message(FATAL_ERROR "sheet delimited exports were incomplete")
endif()

set(SHEET_AST_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-sheet-ast.json")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" ast "${SOURCE_DIR}/examples/sheet_demo.marila"
    RESULT_VARIABLE SHEET_AST_RESULT
    OUTPUT_FILE "${SHEET_AST_OUTPUT}"
    ERROR_VARIABLE SHEET_AST_ERROR
)
if(SHEET_AST_RESULT)
    message(FATAL_ERROR "sheet ast failed: ${SHEET_AST_ERROR}")
endif()
file(READ "${SHEET_AST_OUTPUT}" SHEET_AST_TEXT)
if(NOT SHEET_AST_TEXT MATCHES "\"kind\":\"sheet\"" OR NOT SHEET_AST_TEXT MATCHES "\"reference\":\"C1\"")
    message(FATAL_ERROR "sheet ast did not emit the sheet model")
endif()

set(SHEET_DIAGNOSE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-sheet-diagnose.json")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" diagnose "${SOURCE_DIR}/examples/sheet_demo.marila"
    RESULT_VARIABLE SHEET_DIAGNOSE_RESULT
    OUTPUT_FILE "${SHEET_DIAGNOSE_OUTPUT}"
    ERROR_VARIABLE SHEET_DIAGNOSE_ERROR
)
if(SHEET_DIAGNOSE_RESULT)
    message(FATAL_ERROR "sheet diagnose failed: ${SHEET_DIAGNOSE_ERROR}")
endif()
file(READ "${SHEET_DIAGNOSE_OUTPUT}" SHEET_DIAGNOSE_TEXT)
if(NOT SHEET_DIAGNOSE_TEXT MATCHES "\"ok\":true" OR NOT SHEET_DIAGNOSE_TEXT MATCHES "\"model\":\{\"kind\":\"sheet\"")
    message(FATAL_ERROR "sheet diagnostics did not include the sheet model")
endif()

set(DOCUMENT_AST_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-document-ast.json")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" ast "${SOURCE_DIR}/examples/document_demo.marila"
    RESULT_VARIABLE DOCUMENT_AST_RESULT
    OUTPUT_FILE "${DOCUMENT_AST_OUTPUT}"
    ERROR_VARIABLE DOCUMENT_AST_ERROR
)
if(DOCUMENT_AST_RESULT)
    message(FATAL_ERROR "document ast failed: ${DOCUMENT_AST_ERROR}")
endif()
file(READ "${DOCUMENT_AST_OUTPUT}" DOCUMENT_AST_TEXT)
if(NOT DOCUMENT_AST_TEXT MATCHES "\"kind\":\"text-program\"" OR NOT DOCUMENT_AST_TEXT MATCHES "\"type\":\"document\"" OR NOT DOCUMENT_AST_TEXT MATCHES "\"line\":1")
    message(FATAL_ERROR "document ast did not preserve source lines")
endif()

set(DOCUMENT_DIAGNOSE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-document-diagnose.json")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" diagnose "${SOURCE_DIR}/examples/document_demo.marila"
    RESULT_VARIABLE DOCUMENT_DIAGNOSE_RESULT
    OUTPUT_FILE "${DOCUMENT_DIAGNOSE_OUTPUT}"
    ERROR_VARIABLE DOCUMENT_DIAGNOSE_ERROR
)
if(DOCUMENT_DIAGNOSE_RESULT)
    message(FATAL_ERROR "document diagnose failed: ${DOCUMENT_DIAGNOSE_ERROR}")
endif()
file(READ "${DOCUMENT_DIAGNOSE_OUTPUT}" DOCUMENT_DIAGNOSE_TEXT)
if(NOT DOCUMENT_DIAGNOSE_TEXT MATCHES "\"ok\":true" OR NOT DOCUMENT_DIAGNOSE_TEXT MATCHES "\"model\":\{\"kind\":\"text-program\"")
    message(FATAL_ERROR "document diagnostics did not include the text model")
endif()

set(MARKDOWN_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-document.md")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" export "${SOURCE_DIR}/examples/document_demo.marila" md "${MARKDOWN_OUTPUT}"
    RESULT_VARIABLE MARKDOWN_RESULT
    ERROR_VARIABLE MARKDOWN_ERROR
)
if(MARKDOWN_RESULT)
    message(FATAL_ERROR "Markdown export failed: ${MARKDOWN_ERROR}")
endif()
file(READ "${MARKDOWN_OUTPUT}" MARKDOWN_TEXT)
if(NOT MARKDOWN_TEXT MATCHES "##" OR NOT MARKDOWN_TEXT MATCHES "- ")
    message(FATAL_ERROR "Markdown export did not preserve document structure")
endif()

set(VISUAL_JSON_EXPORT_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-visual-export.json")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" export "${SOURCE_DIR}/examples/scene_geometry_demo.marila" json "${VISUAL_JSON_EXPORT_OUTPUT}"
    RESULT_VARIABLE VISUAL_JSON_EXPORT_RESULT
    ERROR_VARIABLE VISUAL_JSON_EXPORT_ERROR
)
if(VISUAL_JSON_EXPORT_RESULT)
    message(FATAL_ERROR "visual JSON export failed: ${VISUAL_JSON_EXPORT_ERROR}")
endif()
file(READ "${VISUAL_JSON_EXPORT_OUTPUT}" VISUAL_JSON_EXPORT_TEXT)
if(NOT VISUAL_JSON_EXPORT_TEXT MATCHES "\"kind\":\"visual-program\"" OR NOT VISUAL_JSON_EXPORT_TEXT MATCHES "\"nodes\":")
    message(FATAL_ERROR "visual JSON export did not contain the visual AST")
endif()

set(MOTION_AST_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-motion-ast.json")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" ast "${SOURCE_DIR}/examples/motion_demo.marila"
    RESULT_VARIABLE MOTION_AST_RESULT
    OUTPUT_FILE "${MOTION_AST_OUTPUT}"
    ERROR_VARIABLE MOTION_AST_ERROR
)
if(MOTION_AST_RESULT)
    message(FATAL_ERROR "motion ast failed: ${MOTION_AST_ERROR}")
endif()
file(READ "${MOTION_AST_OUTPUT}" MOTION_AST_TEXT)
if(NOT MOTION_AST_TEXT MATCHES "\"timeline_duration\":6")
    message(FATAL_ERROR "motion AST did not expose computed timeline duration: ${MOTION_AST_TEXT}")
endif()

set(MOTION_DIAGNOSE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-motion-diagnose.json")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" diagnose "${SOURCE_DIR}/examples/motion_demo.marila"
    RESULT_VARIABLE MOTION_DIAGNOSE_RESULT
    OUTPUT_FILE "${MOTION_DIAGNOSE_OUTPUT}"
    ERROR_VARIABLE MOTION_DIAGNOSE_ERROR
)
if(MOTION_DIAGNOSE_RESULT)
    message(FATAL_ERROR "motion diagnose failed: ${MOTION_DIAGNOSE_ERROR}")
endif()
file(READ "${MOTION_DIAGNOSE_OUTPUT}" MOTION_DIAGNOSE_TEXT)
if(NOT MOTION_DIAGNOSE_TEXT MATCHES "\"timeline_duration\":6")
    message(FATAL_ERROR "motion diagnostics did not expose timeline duration: ${MOTION_DIAGNOSE_TEXT}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" export "${SOURCE_DIR}/examples/invalid_visual_statement.marila" json "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-invalid-visual-export.json"
    RESULT_VARIABLE INVALID_VISUAL_JSON_RESULT
    ERROR_VARIABLE INVALID_VISUAL_JSON_ERROR
)
if(NOT INVALID_VISUAL_JSON_RESULT OR NOT INVALID_VISUAL_JSON_ERROR MATCHES "unknown visual statement")
    message(FATAL_ERROR "invalid visual JSON export was accepted: ${INVALID_VISUAL_JSON_ERROR}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" ast "${SOURCE_DIR}/examples/invalid_visual_statement.marila"
    RESULT_VARIABLE INVALID_VISUAL_AST_RESULT
    ERROR_VARIABLE INVALID_VISUAL_AST_ERROR
)
if(NOT INVALID_VISUAL_AST_RESULT OR NOT INVALID_VISUAL_AST_ERROR MATCHES "unknown visual statement")
    message(FATAL_ERROR "invalid visual AST was accepted: ${INVALID_VISUAL_AST_ERROR}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" check "${SOURCE_DIR}/examples/invalid_visual_mixed.marila"
    RESULT_VARIABLE INVALID_VISUAL_MIXED_RESULT
    ERROR_VARIABLE INVALID_VISUAL_MIXED_ERROR
)
if(NOT INVALID_VISUAL_MIXED_RESULT OR NOT INVALID_VISUAL_MIXED_ERROR MATCHES "unknown visual statement")
    message(FATAL_ERROR "mixed invalid visual source was accepted: ${INVALID_VISUAL_MIXED_ERROR}")
endif()

set(FINALLY_AST_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-finally-ast.json")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" ast "${SOURCE_DIR}/examples/finally_demo.marila"
    RESULT_VARIABLE FINALLY_AST_RESULT
    OUTPUT_FILE "${FINALLY_AST_OUTPUT}"
    ERROR_VARIABLE FINALLY_AST_ERROR
)
if(FINALLY_AST_RESULT)
    message(FATAL_ERROR "finally ast command failed: ${FINALLY_AST_ERROR}")
endif()
file(READ "${FINALLY_AST_OUTPUT}" FINALLY_AST_TEXT)
if(NOT FINALLY_AST_TEXT MATCHES "\"finally\":\\[" OR NOT FINALLY_AST_TEXT MATCHES "\"line\":5")
    message(FATAL_ERROR "ast output did not preserve finally blocks and source locations: ${FINALLY_AST_TEXT}")
endif()

set(DIAGNOSE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-diagnose.json")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" diagnose "${SOURCE_DIR}/examples/hybrid_program.marila"
    RESULT_VARIABLE DIAGNOSE_RESULT
    OUTPUT_FILE "${DIAGNOSE_OUTPUT}"
    ERROR_VARIABLE DIAGNOSE_ERROR
)
if(DIAGNOSE_RESULT)
    message(FATAL_ERROR "diagnose command failed: ${DIAGNOSE_ERROR}")
endif()
file(READ "${DIAGNOSE_OUTPUT}" DIAGNOSE_TEXT)
if(NOT DIAGNOSE_TEXT MATCHES "\"ok\":true" OR NOT DIAGNOSE_TEXT MATCHES "\"type\":\"program\"")
    message(FATAL_ERROR "diagnose output was unexpected: ${DIAGNOSE_TEXT}")
endif()

set(INVALID_DIAGNOSE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-invalid-diagnose.json")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" diagnose "${SOURCE_DIR}/examples/invalid_program.marila"
    RESULT_VARIABLE INVALID_DIAGNOSE_RESULT
    OUTPUT_FILE "${INVALID_DIAGNOSE_OUTPUT}"
    ERROR_VARIABLE INVALID_DIAGNOSE_ERROR
)
if(NOT INVALID_DIAGNOSE_RESULT)
    message(FATAL_ERROR "diagnose unexpectedly accepted invalid source")
endif()
file(READ "${INVALID_DIAGNOSE_OUTPUT}" INVALID_DIAGNOSE_TEXT)
if(NOT INVALID_DIAGNOSE_TEXT MATCHES "\"ok\":false" OR NOT INVALID_DIAGNOSE_TEXT MATCHES "\"message\":" OR NOT INVALID_DIAGNOSE_TEXT MATCHES "\"line\":3," OR NOT INVALID_DIAGNOSE_TEXT MATCHES "\"column\":14,")
    message(FATAL_ERROR "invalid diagnose output was unexpected: ${INVALID_DIAGNOSE_TEXT}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" check "${SOURCE_DIR}/examples/invalid_trailing_program.marila"
    RESULT_VARIABLE INVALID_TRAILING_RESULT
    ERROR_VARIABLE INVALID_TRAILING_ERROR
)
if(NOT INVALID_TRAILING_RESULT OR NOT INVALID_TRAILING_ERROR MATCHES "unexpected token after program")
    message(FATAL_ERROR "trailing program tokens were not diagnosed: ${INVALID_TRAILING_ERROR}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/invalid_runtime.marila"
    RESULT_VARIABLE INVALID_RUNTIME_RESULT
    ERROR_VARIABLE INVALID_RUNTIME_ERROR
)
if(NOT INVALID_RUNTIME_RESULT OR NOT INVALID_RUNTIME_ERROR MATCHES "line 3, column 1" OR NOT INVALID_RUNTIME_ERROR MATCHES "undefined name 'missing'")
    message(FATAL_ERROR "runtime diagnostics did not include the statement location: ${INVALID_RUNTIME_ERROR}")
endif()

set(VISUAL_AST_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-visual-ast.json")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" ast "${SOURCE_DIR}/examples/scene_geometry_demo.marila"
    RESULT_VARIABLE VISUAL_AST_RESULT
    OUTPUT_FILE "${VISUAL_AST_OUTPUT}"
    ERROR_VARIABLE VISUAL_AST_ERROR
)
if(VISUAL_AST_RESULT)
    message(FATAL_ERROR "visual ast command failed: ${VISUAL_AST_ERROR}")
endif()
file(READ "${VISUAL_AST_OUTPUT}" VISUAL_AST_TEXT)
if(NOT VISUAL_AST_TEXT MATCHES "\"kind\":\"visual-program\"")
    message(FATAL_ERROR "visual ast did not identify a visual program")
endif()
if(NOT VISUAL_AST_TEXT MATCHES "\"kind\":\"triangle\"")
    message(FATAL_ERROR "visual ast did not preserve geometry nodes")
endif()
if(NOT VISUAL_AST_TEXT MATCHES "\"kind\":\"data2d\"" OR NOT VISUAL_AST_TEXT MATCHES "\"samples\":")
    message(FATAL_ERROR "visual ast did not preserve sampled data")
endif()
if(NOT VISUAL_AST_TEXT MATCHES "\"kind\":\"data3d\"" OR NOT VISUAL_AST_TEXT MATCHES "\"grid\":")
    message(FATAL_ERROR "visual ast did not preserve sampled 3D data")
endif()
if(NOT VISUAL_AST_TEXT MATCHES "\"kind\":\"vectorfield2d\"")
    message(FATAL_ERROR "visual ast did not preserve vector field nodes")
endif()
if(NOT VISUAL_AST_TEXT MATCHES "\"animate_trace\":true" OR NOT VISUAL_AST_TEXT MATCHES "\"duration\":2" OR NOT VISUAL_AST_TEXT MATCHES "\"start\":0.5" OR NOT VISUAL_AST_TEXT MATCHES "\"repeat\":2" OR NOT VISUAL_AST_TEXT MATCHES "\"yoyo\":true")
    message(FATAL_ERROR "visual ast did not preserve trace timeline metadata")
endif()
set(VISUAL_COMMENTS_AST_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-visual-comments-ast.json")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" ast "${SOURCE_DIR}/examples/visual_comments_demo.marila"
    RESULT_VARIABLE VISUAL_COMMENTS_AST_RESULT
    OUTPUT_FILE "${VISUAL_COMMENTS_AST_OUTPUT}"
    ERROR_VARIABLE VISUAL_COMMENTS_AST_ERROR
)
if(VISUAL_COMMENTS_AST_RESULT)
    message(FATAL_ERROR "visual comments ast command failed: ${VISUAL_COMMENTS_AST_ERROR}")
endif()
file(READ "${VISUAL_COMMENTS_AST_OUTPUT}" VISUAL_COMMENTS_AST_TEXT)
if(VISUAL_COMMENTS_AST_TEXT MATCHES "Hidden" OR NOT VISUAL_COMMENTS_AST_TEXT MATCHES "\"id\":\"A\"")
    message(FATAL_ERROR "visual comments were not removed before parsing: ${VISUAL_COMMENTS_AST_TEXT}")
endif()
set(FOURD_AST_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-4d-ast.json")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" ast "${SOURCE_DIR}/examples/graphing_4d_demo.marila"
    RESULT_VARIABLE FOURD_AST_RESULT
    OUTPUT_FILE "${FOURD_AST_OUTPUT}"
    ERROR_VARIABLE FOURD_AST_ERROR
)
if(FOURD_AST_RESULT)
    message(FATAL_ERROR "4D ast command failed: ${FOURD_AST_ERROR}")
endif()
file(READ "${FOURD_AST_OUTPUT}" FOURD_AST_TEXT)
if(NOT FOURD_AST_TEXT MATCHES "\"kind\":\"parametric4d\"" OR NOT FOURD_AST_TEXT MATCHES "\"fourth\":\"sin\\(2\\*t\\)\"")
    message(FATAL_ERROR "visual ast did not preserve the fourth 4D coordinate")
endif()

set(EXAMPLES
    document_demo.marila
    calculus_demo.marila
    bracketed_root_demo.marila
    complex_demo.marila
    advanced_linear_algebra_demo.marila
    matrix_exponential_demo.marila
    hyperbolic_demo.marila
    animation_defaults_demo.marila
    video_expression_functions_demo.marila
    statistics_extended_demo.marila
    module_alias_demo.marila
    jacobian_demo.marila
    csv_multiline_demo.marila
    unicode_json_demo.marila
    solvers_demo.marila
    vector_ode_demo.marila
    stacking_demo.marila
    fourier_demo.marila
    fourier_shift_demo.marila
    convolution_demo.marila
    cumulative_integration_demo.marila
    histogram_demo.marila
    interp2_demo.marila
    interp1_modes_demo.marila
    text_transform_demo.marila
    type_introspection_demo.marila
    logspace_demo.marila
    typed_matrix_demo.marila
    typed_function_demo.marila
    polynomial_demo.marila
    vector_polyval_demo.marila
    geometry_demo.marila
    graphs_demo.marila
    hybrid_program.marila
    interop_demo.marila
    latex_demo.marila
    linear_algebra_demo.marila
    line_chart_demo.marila
    json_object_demo.marila
    json_mutation_demo.marila
    higher_order_demo.marila
    assert_demo.marila
    typed_demo.marila
    typed_function_demo.marila
    typed_return_demo.marila
    object_literal_demo.marila
    plot_image_demo.marila
    math_utils.marila
    modules_demo.marila
    scene_geometry_demo.marila
    visual_comments_demo.marila
    sheet_demo.marila
    sheet_mixed_aggregate_demo.marila
    sheet_absolute_demo.marila
    document_escape_demo.marila
    latex_escape_demo.marila
    tikz_demo.marila
    tikz_2d_demo.marila
    video_demo.marila
    ranges_demo.marila
    vector_demo.marila
    vectorized_math_demo.marila
    vectorized_binary_math_demo.marila
    vectorized_complex_demo.marila
    math_functions_demo.marila
    math_extensions_demo.marila
    matmul_demo.marila
    matlab_matrix_demo.marila
    statistics_demo.marila
    text_csv_demo.marila
    static_3d_demo.marila
    text_demo.marila
    text_interop_demo.marila
    motion_demo.marila
    grid_calculus_demo.marila
    matrix_gradient_demo.marila
    matrix_slice_demo.marila
    loop_control_demo.marila
    error_handling_demo.marila
    conditional_demo.marila
    short_circuit_demo.marila
    json_escape_demo.marila
    none_truthy_demo.marila
    iteration_demo.marila
    string_iteration_demo.marila
    unicode_iteration_demo.marila
    finite_predicates_demo.marila
    numeric_predicates_demo.marila
    compound_assignment_demo.marila
    increment_demo.marila
    indexed_compound_demo.marila
    block_comment_demo.marila
    bom_json_demo.marila
    bom_csv_demo.marila
)

foreach(EXAMPLE IN LISTS EXAMPLES)
    execute_process(
        COMMAND "${MARILA_EXECUTABLE}" check "${SOURCE_DIR}/examples/${EXAMPLE}"
        RESULT_VARIABLE CHECK_RESULT
        OUTPUT_VARIABLE CHECK_OUTPUT
        ERROR_VARIABLE CHECK_ERROR
    )
    if(CHECK_RESULT)
        message(FATAL_ERROR "check failed for ${EXAMPLE}: ${CHECK_OUTPUT}${CHECK_ERROR}")
    endif()

    execute_process(
        COMMAND "${MARILA_EXECUTABLE}" lint "${SOURCE_DIR}/examples/${EXAMPLE}"
        RESULT_VARIABLE LINT_RESULT
        OUTPUT_VARIABLE LINT_OUTPUT
        ERROR_VARIABLE LINT_ERROR
    )
    if(LINT_RESULT)
        message(FATAL_ERROR "lint failed for ${EXAMPLE}: ${LINT_OUTPUT}${LINT_ERROR}")
    endif()
endforeach()

set(DEPENDENCY_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-deps.json")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" deps "${SOURCE_DIR}/examples/modules_demo.marila"
    RESULT_VARIABLE DEPENDENCY_RESULT
    OUTPUT_FILE "${DEPENDENCY_OUTPUT}"
    ERROR_VARIABLE DEPENDENCY_ERROR
)
if(DEPENDENCY_RESULT)
    message(FATAL_ERROR "dependency inspection failed: ${DEPENDENCY_ERROR}")
endif()

set(DEPS_DOT_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-deps.dot")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" deps "${SOURCE_DIR}/examples/modules_demo.marila" --dot
    RESULT_VARIABLE DEPS_DOT_RESULT
    OUTPUT_FILE "${DEPS_DOT_OUTPUT}"
    ERROR_VARIABLE DEPS_DOT_ERROR
)
if(DEPS_DOT_RESULT)
    message(FATAL_ERROR "DOT dependency graph failed: ${DEPS_DOT_ERROR}")
endif()
file(READ "${DEPS_DOT_OUTPUT}" DEPS_DOT_TEXT)
if(NOT DEPS_DOT_TEXT MATCHES "digraph marila_dependencies" OR NOT DEPS_DOT_TEXT MATCHES "math_utils.marila")
    message(FATAL_ERROR "DOT dependency graph was incomplete: ${DEPS_DOT_TEXT}")
endif()
file(READ "${DEPENDENCY_OUTPUT}" DEPENDENCY_TEXT)
if(NOT DEPENDENCY_TEXT MATCHES "dependency-graph" OR NOT DEPENDENCY_TEXT MATCHES "math_utils.marila")
    message(FATAL_ERROR "dependency graph did not include the imported module: ${DEPENDENCY_TEXT}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" deps "${SOURCE_DIR}/examples/cycle_a.marila"
    RESULT_VARIABLE CYCLE_DEPENDENCY_RESULT
    ERROR_VARIABLE CYCLE_DEPENDENCY_ERROR
)
if(NOT CYCLE_DEPENDENCY_RESULT)
    message(FATAL_ERROR "dependency inspection accepted a circular import")
endif()

set(COMPLEX_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-complex.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/complex_demo.marila"
    RESULT_VARIABLE COMPLEX_RESULT
    OUTPUT_FILE "${COMPLEX_OUTPUT}"
    ERROR_VARIABLE COMPLEX_ERROR
)
if(COMPLEX_RESULT)
    message(FATAL_ERROR "complex runtime failed: ${COMPLEX_ERROR}")
endif()
file(READ "${COMPLEX_OUTPUT}" COMPLEX_TEXT)
if(NOT COMPLEX_TEXT MATCHES "1 \\+ 2i" OR NOT COMPLEX_TEXT MATCHES "-3 \\+ 4i" OR NOT COMPLEX_TEXT MATCHES "2.236" OR NOT COMPLEX_TEXT MATCHES "1 \\+ 2i")
    message(FATAL_ERROR "complex arithmetic output was unexpected: ${COMPLEX_TEXT}")
endif()

set(ADVANCED_LINEAR_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-advanced-linear.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/advanced_linear_algebra_demo.marila"
    RESULT_VARIABLE ADVANCED_LINEAR_RESULT
    OUTPUT_FILE "${ADVANCED_LINEAR_OUTPUT}"
    ERROR_VARIABLE ADVANCED_LINEAR_ERROR
)
if(ADVANCED_LINEAR_RESULT)
    message(FATAL_ERROR "advanced linear algebra run failed: ${ADVANCED_LINEAR_ERROR}")
endif()
file(READ "${ADVANCED_LINEAR_OUTPUT}" ADVANCED_LINEAR_TEXT)
if(NOT ADVANCED_LINEAR_TEXT MATCHES "2" OR NOT ADVANCED_LINEAR_TEXT MATCHES "1.016" OR NOT ADVANCED_LINEAR_TEXT MATCHES "0.04")
    message(FATAL_ERROR "advanced linear algebra output was unexpected: ${ADVANCED_LINEAR_TEXT}")
endif()

set(STATISTICS_EXTENDED_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-statistics-extended.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/statistics_extended_demo.marila"
    RESULT_VARIABLE STATISTICS_EXTENDED_RESULT
    OUTPUT_FILE "${STATISTICS_EXTENDED_OUTPUT}"
    ERROR_VARIABLE STATISTICS_EXTENDED_ERROR
)
if(STATISTICS_EXTENDED_RESULT)
    message(FATAL_ERROR "extended statistics run failed: ${STATISTICS_EXTENDED_ERROR}")
endif()
file(READ "${STATISTICS_EXTENDED_OUTPUT}" STATISTICS_EXTENDED_TEXT)
if(NOT STATISTICS_EXTENDED_TEXT MATCHES "5" OR NOT STATISTICS_EXTENDED_TEXT MATCHES "14.375" OR NOT STATISTICS_EXTENDED_TEXT MATCHES "\\[1, 2, 8, 64\\]" OR NOT STATISTICS_EXTENDED_TEXT MATCHES "\\[1, 2, 4\\]")
    message(FATAL_ERROR "extended statistics output was unexpected: ${STATISTICS_EXTENDED_TEXT}")
endif()

set(MODULE_ALIAS_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-module-alias.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/module_alias_demo.marila"
    RESULT_VARIABLE MODULE_ALIAS_RESULT
    OUTPUT_FILE "${MODULE_ALIAS_OUTPUT}"
    ERROR_VARIABLE MODULE_ALIAS_ERROR
)
if(MODULE_ALIAS_RESULT)
    message(FATAL_ERROR "module alias run failed: ${MODULE_ALIAS_ERROR}")
endif()
file(READ "${MODULE_ALIAS_OUTPUT}" MODULE_ALIAS_TEXT)
if(NOT MODULE_ALIAS_TEXT MATCHES "36" OR NOT MODULE_ALIAS_TEXT MATCHES "3")
    message(FATAL_ERROR "module alias output was unexpected: ${MODULE_ALIAS_TEXT}")
endif()

set(JACOBIAN_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-jacobian.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/jacobian_demo.marila"
    RESULT_VARIABLE JACOBIAN_RESULT
    OUTPUT_FILE "${JACOBIAN_OUTPUT}"
    ERROR_VARIABLE JACOBIAN_ERROR
)
if(JACOBIAN_RESULT)
    message(FATAL_ERROR "jacobian run failed: ${JACOBIAN_ERROR}")
endif()
file(READ "${JACOBIAN_OUTPUT}" JACOBIAN_TEXT)
if(NOT JACOBIAN_TEXT MATCHES "\\[\\[4, 0\\], \\[0, 1\\]\\]" OR NOT JACOBIAN_TEXT MATCHES "1" OR NOT JACOBIAN_TEXT MATCHES "3" OR NOT JACOBIAN_TEXT MATCHES "\\[3, 2\\]")
    message(FATAL_ERROR "jacobian output was unexpected: ${JACOBIAN_TEXT}")
endif()

set(CSV_MULTILINE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-csv-multiline.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/csv_multiline_demo.marila"
    RESULT_VARIABLE CSV_MULTILINE_RESULT
    OUTPUT_FILE "${CSV_MULTILINE_OUTPUT}"
    ERROR_VARIABLE CSV_MULTILINE_ERROR
)
if(CSV_MULTILINE_RESULT)
    message(FATAL_ERROR "multiline CSV run failed: ${CSV_MULTILINE_ERROR}")
endif()
file(READ "${CSV_MULTILINE_OUTPUT}" CSV_MULTILINE_TEXT)
if(NOT CSV_MULTILINE_TEXT MATCHES "line one" OR NOT CSV_MULTILINE_TEXT MATCHES "line two")
    message(FATAL_ERROR "multiline CSV output was unexpected: ${CSV_MULTILINE_TEXT}")
endif()

set(UNICODE_JSON_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-unicode-json.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/unicode_json_demo.marila"
    RESULT_VARIABLE UNICODE_JSON_RESULT
    OUTPUT_FILE "${UNICODE_JSON_OUTPUT}"
    ERROR_VARIABLE UNICODE_JSON_ERROR
)
if(UNICODE_JSON_RESULT)
    message(FATAL_ERROR "unicode JSON run failed: ${UNICODE_JSON_ERROR}")
endif()
file(READ "${UNICODE_JSON_OUTPUT}" UNICODE_JSON_TEXT)
if(NOT UNICODE_JSON_TEXT MATCHES "café" OR NOT UNICODE_JSON_TEXT MATCHES "😀")
    message(FATAL_ERROR "unicode JSON output was unexpected: ${UNICODE_JSON_TEXT}")
endif()

set(PROGRAM_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-program.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/hybrid_program.marila"
    RESULT_VARIABLE RUN_RESULT
    OUTPUT_FILE "${PROGRAM_OUTPUT}"
    ERROR_VARIABLE RUN_ERROR
)
if(RUN_RESULT)
    message(FATAL_ERROR "program run failed: ${RUN_ERROR}")
endif()
file(READ "${PROGRAM_OUTPUT}" PROGRAM_TEXT)
if(NOT PROGRAM_TEXT MATCHES "\\[\\[4, 4\\], \\[10, 8\\]\\]")
    message(FATAL_ERROR "program output did not contain expected matrix: ${PROGRAM_TEXT}")
endif()

set(NONE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-none.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/none_truthy_demo.marila"
    RESULT_VARIABLE NONE_RESULT
    OUTPUT_FILE "${NONE_OUTPUT}"
    ERROR_VARIABLE NONE_ERROR
)
if(NONE_RESULT)
    message(FATAL_ERROR "none truthiness run failed: ${NONE_ERROR}")
endif()
file(READ "${NONE_OUTPUT}" NONE_TEXT)
if(NOT NONE_TEXT MATCHES "none is false")
    message(FATAL_ERROR "none should be falsey: ${NONE_TEXT}")
endif()

set(MATH_EXTENSIONS_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-math-extensions.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/math_extensions_demo.marila"
    RESULT_VARIABLE MATH_EXTENSIONS_RESULT
    OUTPUT_FILE "${MATH_EXTENSIONS_OUTPUT}"
    ERROR_VARIABLE MATH_EXTENSIONS_ERROR
)
if(MATH_EXTENSIONS_RESULT)
    message(FATAL_ERROR "math extension run failed: ${MATH_EXTENSIONS_ERROR}")
endif()
file(READ "${MATH_EXTENSIONS_OUTPUT}" MATH_EXTENSIONS_TEXT)
if(NOT MATH_EXTENSIONS_TEXT MATCHES "0.785398")
    message(FATAL_ERROR "atan output was unexpected: ${MATH_EXTENSIONS_TEXT}")
endif()
if(NOT MATH_EXTENSIONS_TEXT MATCHES "\n3\n32\n-1")
    message(FATAL_ERROR "math extension output was unexpected: ${MATH_EXTENSIONS_TEXT}")
endif()

set(TEXT_INTEROP_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-text-interop.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/text_interop_demo.marila"
    RESULT_VARIABLE TEXT_INTEROP_RESULT
    OUTPUT_FILE "${TEXT_INTEROP_OUTPUT}"
    ERROR_VARIABLE TEXT_INTEROP_ERROR
)
if(TEXT_INTEROP_RESULT)
    message(FATAL_ERROR "text interoperability run failed: ${TEXT_INTEROP_ERROR}")
endif()
file(READ "${TEXT_INTEROP_OUTPUT}" TEXT_INTEROP_TEXT)
if(NOT TEXT_INTEROP_TEXT MATCHES "alpha" OR NOT TEXT_INTEROP_TEXT MATCHES "2.5" OR NOT TEXT_INTEROP_TEXT MATCHES "Marila")
    message(FATAL_ERROR "text interoperability output was unexpected: ${TEXT_INTEROP_TEXT}")
endif()

set(VECTORIZED_MATH_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-vectorized-math.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/vectorized_math_demo.marila"
    RESULT_VARIABLE VECTORIZED_MATH_RESULT
    OUTPUT_FILE "${VECTORIZED_MATH_OUTPUT}"
    ERROR_VARIABLE VECTORIZED_MATH_ERROR
)
if(VECTORIZED_MATH_RESULT)
    message(FATAL_ERROR "vectorized math run failed: ${VECTORIZED_MATH_ERROR}")
endif()
file(READ "${VECTORIZED_MATH_OUTPUT}" VECTORIZED_MATH_TEXT)
if(NOT VECTORIZED_MATH_TEXT MATCHES "\\[0, 1, 1.22465e-16\\]" OR NOT VECTORIZED_MATH_TEXT MATCHES "\\[1, 2, 3\\]" OR NOT VECTORIZED_MATH_TEXT MATCHES "7.38906")
    message(FATAL_ERROR "vectorized math output was unexpected: ${VECTORIZED_MATH_TEXT}")
endif()
set(VECTORIZED_BINARY_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-vectorized-binary.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/vectorized_binary_math_demo.marila"
    RESULT_VARIABLE VECTORIZED_BINARY_RESULT
    OUTPUT_FILE "${VECTORIZED_BINARY_OUTPUT}"
    ERROR_VARIABLE VECTORIZED_BINARY_ERROR
)
if(VECTORIZED_BINARY_RESULT)
    message(FATAL_ERROR "vectorized binary math run failed: ${VECTORIZED_BINARY_ERROR}")
endif()
file(READ "${VECTORIZED_BINARY_OUTPUT}" VECTORIZED_BINARY_TEXT)
string(FIND "${VECTORIZED_BINARY_TEXT}" "1 + 3i" VECTORIZED_COMPLEX_POSITION)
if(NOT VECTORIZED_BINARY_TEXT MATCHES "\\[1, 4, 9\\]" OR NOT VECTORIZED_BINARY_TEXT MATCHES "\\[5, 6.40312\\]" OR NOT VECTORIZED_BINARY_TEXT MATCHES "1.5708" OR VECTORIZED_COMPLEX_POSITION EQUAL -1)
    message(FATAL_ERROR "vectorized binary math output was unexpected: ${VECTORIZED_BINARY_TEXT}")
endif()
set(VECTORIZED_COMPLEX_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-vectorized-complex.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/vectorized_complex_demo.marila"
    RESULT_VARIABLE VECTORIZED_COMPLEX_RESULT
    OUTPUT_FILE "${VECTORIZED_COMPLEX_OUTPUT}"
    ERROR_VARIABLE VECTORIZED_COMPLEX_ERROR
)
if(VECTORIZED_COMPLEX_RESULT)
    message(FATAL_ERROR "vectorized complex run failed: ${VECTORIZED_COMPLEX_ERROR}")
endif()
file(READ "${VECTORIZED_COMPLEX_OUTPUT}" VECTORIZED_COMPLEX_TEXT)
if(NOT VECTORIZED_COMPLEX_TEXT MATCHES "\\[1, 0\\]" OR NOT VECTORIZED_COMPLEX_TEXT MATCHES "\\[0, 1\\]" OR NOT VECTORIZED_COMPLEX_TEXT MATCHES "1.5708")
    message(FATAL_ERROR "vectorized complex output was unexpected: ${VECTORIZED_COMPLEX_TEXT}")
endif()

set(SHEET_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-sheet.html")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" render "${SOURCE_DIR}/examples/sheet_demo.marila" "${SHEET_OUTPUT}"
    RESULT_VARIABLE SHEET_RESULT
    ERROR_VARIABLE SHEET_ERROR
)
if(SHEET_RESULT)
    message(FATAL_ERROR "sheet render failed: ${SHEET_ERROR}")
endif()
file(READ "${SHEET_OUTPUT}" SHEET_TEXT)
if(NOT SHEET_TEXT MATCHES "data-cell=\"C1\">30")
    message(FATAL_ERROR "sheet output did not contain expected C1 value")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"E1\">1")
    message(FATAL_ERROR "sheet output did not evaluate IF formula")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"F1\">3")
    message(FATAL_ERROR "sheet output did not evaluate COUNT formula")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"G1\">12")
    message(FATAL_ERROR "sheet output did not evaluate MIN formula")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"H1\">30")
    message(FATAL_ERROR "sheet output did not evaluate MAX formula")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"AQ1\">18")
    message(FATAL_ERROR "sheet output did not evaluate INDEX formula")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"AR1\">2")
    message(FATAL_ERROR "sheet output did not evaluate MATCH formula")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"AS1\">18")
    message(FATAL_ERROR "sheet output did not evaluate VLOOKUP formula")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"AT1\">18")
    message(FATAL_ERROR "sheet output did not evaluate HLOOKUP formula")
endif()
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" render "${SOURCE_DIR}/examples/invalid_formula_sheet.marila" "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-invalid-formula.html"
    RESULT_VARIABLE INVALID_FORMULA_RESULT
    ERROR_VARIABLE INVALID_FORMULA_ERROR
)
if(INVALID_FORMULA_RESULT)
    message(FATAL_ERROR "invalid spreadsheet render failed unexpectedly: ${INVALID_FORMULA_ERROR}")
endif()
file(READ "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-invalid-formula.html" INVALID_FORMULA_TEXT)
if(NOT INVALID_FORMULA_TEXT MATCHES "#ERROR: unsupported spreadsheet formula")
    message(FATAL_ERROR "unsupported spreadsheet formulas were silently accepted: ${INVALID_FORMULA_TEXT}")
endif()
if(NOT INVALID_FORMULA_TEXT MATCHES "#ERROR: unknown spreadsheet name")
    message(FATAL_ERROR "unknown spreadsheet names were silently accepted: ${INVALID_FORMULA_TEXT}")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"I1\">60")
    message(FATAL_ERROR "sheet output did not evaluate parenthesized arithmetic")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"J1\">-24")
    message(FATAL_ERROR "sheet output did not evaluate unary negative arithmetic")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"K1\">18")
    message(FATAL_ERROR "sheet output did not evaluate MEDIAN")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"L1\">7.48331")
    message(FATAL_ERROR "sheet output did not evaluate STDEV")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"M1\">60")
    message(FATAL_ERROR "sheet output did not evaluate multi-argument SUM")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"N1\">20")
    message(FATAL_ERROR "sheet output did not evaluate multi-argument AVERAGE")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"O1\">3")
    message(FATAL_ERROR "sheet output did not evaluate ROUND")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"P1\">4")
    message(FATAL_ERROR "sheet output did not evaluate SQRT")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"Q1\">32")
    message(FATAL_ERROR "sheet output did not evaluate POWER")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"R1\">62")
    message(FATAL_ERROR "sheet output did not combine ranges and scalar arguments")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"S1\">1")
    message(FATAL_ERROR "sheet output did not evaluate AND")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"T1\">1")
    message(FATAL_ERROR "sheet output did not evaluate OR")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"U1\">1")
    message(FATAL_ERROR "sheet output did not evaluate NOT")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"V1\">18")
    message(FATAL_ERROR "sheet output did not evaluate multi-argument MEDIAN")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"W1\">7.48331")
    message(FATAL_ERROR "sheet output did not evaluate multi-argument STDEV")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"X1\">3")
    message(FATAL_ERROR "sheet output did not evaluate LOG2")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"Y1\">3.14")
    message(FATAL_ERROR "sheet output did not evaluate precision-aware ROUND")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"Z1\">48")
    message(FATAL_ERROR "sheet output did not evaluate SUMIF")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"AA1\">2")
    message(FATAL_ERROR "sheet output did not evaluate COUNTIF")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"AF1\">24")
    message(FATAL_ERROR "sheet output did not evaluate AVERAGEIF")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"AG1\">18" OR NOT SHEET_TEXT MATCHES "data-cell=\"AH1\">1" OR NOT SHEET_TEXT MATCHES "data-cell=\"AI1\">18")
    message(FATAL_ERROR "sheet output did not evaluate multi-criteria aggregates")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"AJ1\">18" OR NOT SHEET_TEXT MATCHES "data-cell=\"AK1\">18")
    message(FATAL_ERROR "sheet output did not evaluate MINIFS/MAXIFS")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"AL1\">99")
    message(FATAL_ERROR "sheet output did not evaluate nested IF formulas")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"AM1\">7")
    message(FATAL_ERROR "sheet output did not evaluate IFERROR for division by zero")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"AN1\">1" OR NOT SHEET_TEXT MATCHES "data-cell=\"AO1\">1" OR NOT SHEET_TEXT MATCHES "data-cell=\"AP1\">1")
    message(FATAL_ERROR "sheet output did not evaluate cell type predicates")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"AB1\">6480")
    message(FATAL_ERROR "sheet output did not evaluate PRODUCT")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"AC1\">1368")
    message(FATAL_ERROR "sheet output did not evaluate SUMPRODUCT")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"AD1\">2")
    message(FATAL_ERROR "sheet output did not evaluate MOD")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"AE1\">1")
    message(FATAL_ERROR "sheet output did not evaluate text COUNTIF")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"F2\">0")
    message(FATAL_ERROR "sheet output did not exclude text from COUNT")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"G2\">3")
    message(FATAL_ERROR "sheet output did not evaluate COUNTA")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"H2\">1")
    message(FATAL_ERROR "sheet output did not evaluate COUNTBLANK")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"A2\">Name")
    message(FATAL_ERROR "sheet output did not preserve text cells")
endif()
if(NOT SHEET_TEXT MATCHES "data-cell=\"C2\">quoted &quot;cell&quot;")
    message(FATAL_ERROR "sheet output did not unescape quoted text cells")
endif()
if(NOT SHEET_TEXT MATCHES "class=\"chart\"")
    message(FATAL_ERROR "sheet output did not contain expected chart")
endif()
set(SHEET_MIXED_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-sheet-mixed.html")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" render "${SOURCE_DIR}/examples/sheet_mixed_aggregate_demo.marila" "${SHEET_MIXED_OUTPUT}"
    RESULT_VARIABLE SHEET_MIXED_RESULT
    ERROR_VARIABLE SHEET_MIXED_ERROR
)
if(SHEET_MIXED_RESULT)
    message(FATAL_ERROR "mixed spreadsheet aggregate render failed: ${SHEET_MIXED_ERROR}")
endif()
file(READ "${SHEET_MIXED_OUTPUT}" SHEET_MIXED_TEXT)
if(NOT SHEET_MIXED_TEXT MATCHES "data-cell=\"D1\">35" OR NOT SHEET_MIXED_TEXT MATCHES "data-cell=\"E1\">3" OR NOT SHEET_MIXED_TEXT MATCHES "data-cell=\"F1\">4" OR NOT SHEET_MIXED_TEXT MATCHES "data-cell=\"G1\">4")
    message(FATAL_ERROR "mixed spreadsheet aggregates did not ignore text and count blanks correctly")
endif()
set(SHEET_ABSOLUTE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-sheet-absolute.html")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" render "${SOURCE_DIR}/examples/sheet_absolute_demo.marila" "${SHEET_ABSOLUTE_OUTPUT}"
    RESULT_VARIABLE SHEET_ABSOLUTE_RESULT
    ERROR_VARIABLE SHEET_ABSOLUTE_ERROR
)
if(SHEET_ABSOLUTE_RESULT)
    message(FATAL_ERROR "absolute spreadsheet reference render failed: ${SHEET_ABSOLUTE_ERROR}")
endif()
file(READ "${SHEET_ABSOLUTE_OUTPUT}" SHEET_ABSOLUTE_TEXT)
if(NOT SHEET_ABSOLUTE_TEXT MATCHES "data-cell=\"C1\">15" OR NOT SHEET_ABSOLUTE_TEXT MATCHES "data-cell=\"D1\">15")
    message(FATAL_ERROR "absolute spreadsheet references were not evaluated: ${SHEET_ABSOLUTE_TEXT}")
endif()
set(LINE_CHART_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-line-chart.html")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" render "${SOURCE_DIR}/examples/line_chart_demo.marila" "${LINE_CHART_OUTPUT}"
    RESULT_VARIABLE LINE_CHART_RESULT
    ERROR_VARIABLE LINE_CHART_ERROR
)
if(LINE_CHART_RESULT)
    message(FATAL_ERROR "line chart render failed: ${LINE_CHART_ERROR}")
endif()
file(READ "${LINE_CHART_OUTPUT}" LINE_CHART_TEXT)
if(NOT LINE_CHART_TEXT MATCHES "<polyline")
    message(FATAL_ERROR "line chart output did not contain expected polyline")
endif()
if(NOT LINE_CHART_TEXT MATCHES "<rect")
    message(FATAL_ERROR "multi-chart output did not contain expected bar chart")
endif()
if(NOT LINE_CHART_TEXT MATCHES "Trend" OR NOT LINE_CHART_TEXT MATCHES "Distribution")
    message(FATAL_ERROR "chart output did not preserve chart titles")
endif()
if(NOT LINE_CHART_TEXT MATCHES "Samples" OR NOT LINE_CHART_TEXT MATCHES "r=\"5\"")
    message(FATAL_ERROR "scatter chart output was not rendered")
endif()

set(GRAPH_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-graphs.html")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" render "${SOURCE_DIR}/examples/graphs_demo.marila" "${GRAPH_OUTPUT}"
    RESULT_VARIABLE GRAPH_RESULT
    ERROR_VARIABLE GRAPH_ERROR
)
if(GRAPH_RESULT)
    message(FATAL_ERROR "graph render failed: ${GRAPH_ERROR}")
endif()
file(READ "${GRAPH_OUTPUT}" GRAPH_TEXT)
if(NOT GRAPH_TEXT MATCHES "Math.exp\\(u\\)")
    message(FATAL_ERROR "graph output did not preserve exp expression")
endif()
if(NOT GRAPH_TEXT MATCHES "type:'param2d'")
    message(FATAL_ERROR "graph output did not contain parametric plot")
endif()
if(NOT GRAPH_TEXT MATCHES "type:'implicit2d'")
    message(FATAL_ERROR "graph output did not contain implicit plot")
endif()
if(NOT GRAPH_TEXT MATCHES "Math.cos\\(u\\)")
    message(FATAL_ERROR "graph output did not contain polar curve coordinates")
endif()
if(NOT GRAPH_TEXT MATCHES "trace:true")
    message(FATAL_ERROR "graph output did not preserve animate trace modifier")
endif()
if(NOT GRAPH_TEXT MATCHES "xmin:-3.14159")
    message(FATAL_ERROR "pi-based graph domain was not parsed")
endif()
if(NOT GRAPH_TEXT MATCHES "xmax:6.28319")
    message(FATAL_ERROR "arithmetic graph domain endpoint was not parsed")
endif()
if(NOT GRAPH_TEXT MATCHES "Math.asin\\(u\\)")
    message(FATAL_ERROR "graph output did not translate inverse sine")
endif()
if(NOT GRAPH_TEXT MATCHES "Math.log10\\(u\\)")
    message(FATAL_ERROR "graph output did not translate base-10 logarithm")
endif()
if(NOT GRAPH_TEXT MATCHES "Math.atan\\(u\\)")
    message(FATAL_ERROR "graph output did not translate inverse tangent")
endif()
if(NOT GRAPH_TEXT MATCHES "type:'vector2d'.*Math.cos")
    message(FATAL_ERROR "graph output did not preserve vector field")
endif()
if(NOT GRAPH_TEXT MATCHES "Inverse sine")
    message(FATAL_ERROR "graph output did not preserve plot title")
endif()
if(NOT GRAPH_TEXT MATCHES "class=\"plot-title\"")
    message(FATAL_ERROR "graph output did not render plot title markup")
endif()
if(NOT GRAPH_TEXT MATCHES "color:\"red\"")
    message(FATAL_ERROR "graph output did not preserve plot color")
endif()

set(SCENE_GEOMETRY_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-scene-geometry.html")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" render "${SOURCE_DIR}/examples/scene_geometry_demo.marila" "${SCENE_GEOMETRY_OUTPUT}"
    RESULT_VARIABLE SCENE_GEOMETRY_RESULT
    ERROR_VARIABLE SCENE_GEOMETRY_ERROR
)
if(SCENE_GEOMETRY_RESULT)
    message(FATAL_ERROR "scene geometry render failed: ${SCENE_GEOMETRY_ERROR}")
endif()
file(READ "${SCENE_GEOMETRY_OUTPUT}" SCENE_GEOMETRY_TEXT)
if(NOT SCENE_GEOMETRY_TEXT MATCHES "type:'line'.*type:'triangle'.*type:'circle'")
    message(FATAL_ERROR "interactive scene did not preserve geometry nodes")
endif()
if(NOT SCENE_GEOMETRY_TEXT MATCHES "type:'data2d'.*Samples")
    message(FATAL_ERROR "interactive scene did not preserve sampled data")
endif()
if(NOT SCENE_GEOMETRY_TEXT MATCHES "type:'data3d'.*Measured")
    message(FATAL_ERROR "interactive scene did not preserve sampled 3D data")
endif()

set(TIKZ_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-tikz.tex")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" render "${SOURCE_DIR}/examples/tikz_demo.marila" "${TIKZ_OUTPUT}"
    RESULT_VARIABLE TIKZ_RESULT
    ERROR_VARIABLE TIKZ_ERROR
)
if(TIKZ_RESULT)
    message(FATAL_ERROR "TikZ render failed: ${TIKZ_ERROR}")
endif()
file(READ "${TIKZ_OUTPUT}" TIKZ_TEXT)
if(NOT TIKZ_TEXT MATCHES "\\\\coordinate \\(A\\)")
    message(FATAL_ERROR "TikZ output did not preserve shared point A")
endif()
if(NOT TIKZ_TEXT MATCHES "\\\\node\\[below\\]")
    message(FATAL_ERROR "TikZ output did not preserve point labels")
endif()
if(NOT TIKZ_TEXT MATCHES "plot")
    message(FATAL_ERROR "TikZ output did not preserve function plots")
endif()
if(NOT TIKZ_TEXT MATCHES "sin\\(\\\\x r\\)")
    message(FATAL_ERROR "TikZ output did not preserve radian trigonometry")
endif()
if(NOT TIKZ_TEXT MATCHES "\\\\usepackage\\{pgfplots\\}")
    message(FATAL_ERROR "TikZ output did not enable 3D plots")
endif()
if(NOT TIKZ_TEXT MATCHES "\\\\addplot3")
    message(FATAL_ERROR "TikZ output did not contain a 3D plot")
endif()
if(NOT TIKZ_TEXT MATCHES "plot coordinates")
    message(FATAL_ERROR "TikZ output did not preserve sampled data")
endif()
if(NOT TIKZ_TEXT MATCHES "coordinates \\{\\(0,0,0\\)")
    message(FATAL_ERROR "TikZ output did not preserve sampled 3D data")
endif()
if(NOT TIKZ_TEXT MATCHES "draw.*->")
    message(FATAL_ERROR "TikZ output did not preserve vector field")
endif()

set(TIKZ_2D_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-tikz-2d.tex")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" render "${SOURCE_DIR}/examples/tikz_2d_demo.marila" "${TIKZ_2D_OUTPUT}"
    RESULT_VARIABLE TIKZ_2D_RESULT
    ERROR_VARIABLE TIKZ_2D_ERROR
)
if(TIKZ_2D_RESULT)
    message(FATAL_ERROR "2D TikZ render failed: ${TIKZ_2D_ERROR}")
endif()
file(READ "${TIKZ_2D_OUTPUT}" TIKZ_2D_TEXT)
if(NOT TIKZ_2D_TEXT MATCHES "\\\\coordinate \\(A\\)" OR NOT TIKZ_2D_TEXT MATCHES "plot" OR NOT TIKZ_2D_TEXT MATCHES "color=red" OR NOT TIKZ_2D_TEXT MATCHES "Sine")
    message(FATAL_ERROR "2D TikZ output was incomplete")
endif()

set(LATEX_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-latex.tex")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" render "${SOURCE_DIR}/examples/latex_demo.marila" "${LATEX_OUTPUT}"
    RESULT_VARIABLE LATEX_RESULT
    ERROR_VARIABLE LATEX_ERROR
)
if(LATEX_RESULT)
    message(FATAL_ERROR "LaTeX render failed: ${LATEX_ERROR}")
endif()
file(READ "${LATEX_OUTPUT}" LATEX_TEXT)
if(NOT LATEX_TEXT MATCHES "\\\\usepackage\\{amsmath,amssymb\\}")
    message(FATAL_ERROR "LaTeX output did not contain the math package")
endif()
if(NOT LATEX_TEXT MATCHES "\\\\usepackage\\{hyperref\\}" OR NOT LATEX_TEXT MATCHES "\\\\section\\{Foundations\\}" OR NOT LATEX_TEXT MATCHES "\\\\subsection\\{Definitions\\}" OR NOT LATEX_TEXT MATCHES "\\\\href")
    message(FATAL_ERROR "LaTeX output did not preserve semantic document structure")
endif()
if(NOT LATEX_TEXT MATCHES "\\\\int_\\{0\\}")
    message(FATAL_ERROR "LaTeX output did not preserve integral notation")
endif()
if(NOT LATEX_TEXT MATCHES "\\\\usepackage\\{tikz\\}")
    message(FATAL_ERROR "LaTeX output did not embed shared geometry")
endif()
if(NOT LATEX_TEXT MATCHES "domain=-3.140000:3.140000")
    message(FATAL_ERROR "LaTeX output did not embed plot commands")
endif()
if(NOT LATEX_TEXT MATCHES "\\\\usepackage\\{pgfplots\\}")
    message(FATAL_ERROR "LaTeX output did not enable 3D plots")
endif()
if(NOT LATEX_TEXT MATCHES "\\\\addplot3")
    message(FATAL_ERROR "LaTeX output did not embed a 3D plot")
endif()
if(NOT LATEX_TEXT MATCHES "Measured")
    message(FATAL_ERROR "LaTeX output did not preserve sampled 3D data metadata")
endif()
if(NOT LATEX_TEXT MATCHES "draw.*->")
    message(FATAL_ERROR "LaTeX output did not preserve vector field")
endif()

set(LATEX_ESCAPE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-latex-escape.tex")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" render "${SOURCE_DIR}/examples/latex_escape_demo.marila" "${LATEX_ESCAPE_OUTPUT}"
    RESULT_VARIABLE LATEX_ESCAPE_RESULT
    ERROR_VARIABLE LATEX_ESCAPE_ERROR
)
if(LATEX_ESCAPE_RESULT)
    message(FATAL_ERROR "escaped LaTeX render failed: ${LATEX_ESCAPE_ERROR}")
endif()
file(READ "${LATEX_ESCAPE_OUTPUT}" LATEX_ESCAPE_TEXT)
if(NOT LATEX_ESCAPE_TEXT MATCHES "quoted" OR NOT LATEX_ESCAPE_TEXT MATCHES "Author")
    message(FATAL_ERROR "LaTeX renderer did not decode escaped text: ${LATEX_ESCAPE_TEXT}")
endif()

set(GEOMETRY_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-geometry.svg")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" render "${SOURCE_DIR}/examples/geometry_demo.marila" "${GEOMETRY_OUTPUT}"
    RESULT_VARIABLE GEOMETRY_RESULT
    ERROR_VARIABLE GEOMETRY_ERROR
)
if(GEOMETRY_RESULT)
    message(FATAL_ERROR "geometry render failed: ${GEOMETRY_ERROR}")
endif()
file(READ "${GEOMETRY_OUTPUT}" GEOMETRY_TEXT)
if(NOT GEOMETRY_TEXT MATCHES "stroke=\"#f59e0b\"")
    message(FATAL_ERROR "geometry output did not preserve amber triangle color")
endif()
if(NOT GEOMETRY_TEXT MATCHES "90°")
    message(FATAL_ERROR "geometry output did not contain computed angle labels")
endif()

set(PLOT_IMAGE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-plot-image.svg")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" render "${SOURCE_DIR}/examples/plot_image_demo.marila" "${PLOT_IMAGE_OUTPUT}"
    RESULT_VARIABLE PLOT_IMAGE_RESULT
    ERROR_VARIABLE PLOT_IMAGE_ERROR
)
if(PLOT_IMAGE_RESULT)
    message(FATAL_ERROR "plot image render failed: ${PLOT_IMAGE_ERROR}")
endif()
file(READ "${PLOT_IMAGE_OUTPUT}" PLOT_IMAGE_TEXT)
if(NOT PLOT_IMAGE_TEXT MATCHES "<path")
    message(FATAL_ERROR "plot image output did not contain sampled paths")
endif()
if(NOT PLOT_IMAGE_TEXT MATCHES "stroke=\"#ef4444\"")
    message(FATAL_ERROR "plot image output did not preserve plot color")
endif()
if(NOT PLOT_IMAGE_TEXT MATCHES "data-plot=\"implicit\"")
    message(FATAL_ERROR "plot image output did not contain implicit plot samples")
endif()
if(NOT PLOT_IMAGE_TEXT MATCHES "data-plot=\"data2d\"")
    message(FATAL_ERROR "plot image output did not contain sampled data")
endif()
if(NOT PLOT_IMAGE_TEXT MATCHES "<title>Samples</title>")
    message(FATAL_ERROR "plot image output did not preserve sampled plot title")
endif()
if(NOT PLOT_IMAGE_TEXT MATCHES "data-plot=\"vectorfield\"")
    message(FATAL_ERROR "plot image output did not contain vector field samples")
endif()

set(STATIC_3D_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-static-3d.svg")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" render "${SOURCE_DIR}/examples/static_3d_demo.marila" "${STATIC_3D_OUTPUT}"
    RESULT_VARIABLE STATIC_3D_RESULT
    ERROR_VARIABLE STATIC_3D_ERROR
)
if(STATIC_3D_RESULT)
    message(FATAL_ERROR "static 3D render failed: ${STATIC_3D_ERROR}")
endif()
file(READ "${STATIC_3D_OUTPUT}" STATIC_3D_TEXT)
if(NOT STATIC_3D_TEXT MATCHES "data-plot=\"3d\"")
    message(FATAL_ERROR "static 3D output did not contain a sampled surface")
endif()
if(NOT STATIC_3D_TEXT MATCHES "stroke=\"#22c55e\"")
    message(FATAL_ERROR "static 3D output did not preserve surface color")
endif()
if(NOT STATIC_3D_TEXT MATCHES "data-solid=\"sphere\"")
    message(FATAL_ERROR "static 3D output did not contain a sphere")
endif()
if(NOT STATIC_3D_TEXT MATCHES "data-solid=\"cube\"")
    message(FATAL_ERROR "static 3D output did not contain a cube")
endif()
if(NOT STATIC_3D_TEXT MATCHES "data-plot=\"data3d\"")
    message(FATAL_ERROR "static 3D output did not contain sampled surface data")
endif()

set(OBJ_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-static-3d.obj")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" export "${SOURCE_DIR}/examples/static_3d_demo.marila" obj "${OBJ_OUTPUT}"
    RESULT_VARIABLE OBJ_RESULT
    ERROR_VARIABLE OBJ_ERROR
)
if(OBJ_RESULT)
    message(FATAL_ERROR "OBJ export failed: ${OBJ_ERROR}")
endif()
file(READ "${OBJ_OUTPUT}" OBJ_TEXT)
if(NOT OBJ_TEXT MATCHES "# Marila Wavefront OBJ export" OR NOT OBJ_TEXT MATCHES "v " OR NOT OBJ_TEXT MATCHES "f ")
    message(FATAL_ERROR "OBJ export did not contain vertices and faces")
endif()
set(EXTENDED_OBJ_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-extended.obj")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" export "${SOURCE_DIR}/examples/export_extended_visuals.marila" obj "${EXTENDED_OBJ_OUTPUT}"
    RESULT_VARIABLE EXTENDED_OBJ_RESULT
    ERROR_VARIABLE EXTENDED_OBJ_ERROR
)
if(EXTENDED_OBJ_RESULT)
    message(FATAL_ERROR "extended visual OBJ export failed: ${EXTENDED_OBJ_ERROR}")
endif()
file(READ "${EXTENDED_OBJ_OUTPUT}" EXTENDED_OBJ_TEXT)
if(NOT EXTENDED_OBJ_TEXT MATCHES "v " OR NOT EXTENDED_OBJ_TEXT MATCHES "l ")
    message(FATAL_ERROR "extended visual OBJ export did not preserve projected line geometry")
endif()

set(STL_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-static-3d.stl")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" export "${SOURCE_DIR}/examples/static_3d_demo.marila" stl "${STL_OUTPUT}"
    RESULT_VARIABLE STL_RESULT
    ERROR_VARIABLE STL_ERROR
)
if(STL_RESULT)
    message(FATAL_ERROR "STL export failed: ${STL_ERROR}")
endif()
file(READ "${STL_OUTPUT}" STL_TEXT)
if(NOT STL_TEXT MATCHES "solid marila" OR NOT STL_TEXT MATCHES "facet normal" OR NOT STL_TEXT MATCHES "endsolid marila")
    message(FATAL_ERROR "STL export did not contain a valid ASCII solid")
endif()

set(PLY_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-static-3d.ply")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" export "${SOURCE_DIR}/examples/static_3d_demo.marila" ply "${PLY_OUTPUT}"
    RESULT_VARIABLE PLY_RESULT
    ERROR_VARIABLE PLY_ERROR
)
if(PLY_RESULT)
    message(FATAL_ERROR "PLY export failed: ${PLY_ERROR}")
endif()
file(READ "${PLY_OUTPUT}" PLY_TEXT)
if(NOT PLY_TEXT MATCHES "ply" OR NOT PLY_TEXT MATCHES "format ascii 1.0" OR NOT PLY_TEXT MATCHES "element vertex" OR NOT PLY_TEXT MATCHES "element face")
    message(FATAL_ERROR "PLY export did not contain a valid ASCII mesh")
endif()

set(GLTF_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-static-3d.gltf")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" export "${SOURCE_DIR}/examples/static_3d_demo.marila" gltf "${GLTF_OUTPUT}"
    RESULT_VARIABLE GLTF_RESULT
    ERROR_VARIABLE GLTF_ERROR
)
if(GLTF_RESULT)
    message(FATAL_ERROR "glTF export failed: ${GLTF_ERROR}")
endif()
file(READ "${GLTF_OUTPUT}" GLTF_TEXT)
if(NOT GLTF_TEXT MATCHES "\\\"version\\\":\\\"2.0\\\"" OR NOT GLTF_TEXT MATCHES "data:application/octet-stream;base64" OR NOT GLTF_TEXT MATCHES "\\\"primitives\\\"")
    message(FATAL_ERROR "glTF export did not contain a self-contained 2.0 mesh")
endif()

find_program(FFMPEG_EXECUTABLE ffmpeg)
if(FFMPEG_EXECUTABLE)
    set(VIDEO_EXPORT_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-video.mp4")
    execute_process(
        COMMAND "${MARILA_EXECUTABLE}" export "${SOURCE_DIR}/examples/video_demo.marila" mp4 "${VIDEO_EXPORT_OUTPUT}"
        RESULT_VARIABLE VIDEO_EXPORT_RESULT
        ERROR_VARIABLE VIDEO_EXPORT_ERROR
    )
    if(VIDEO_EXPORT_RESULT)
        message(FATAL_ERROR "video export failed: ${VIDEO_EXPORT_ERROR}")
    endif()
    file(SIZE "${VIDEO_EXPORT_OUTPUT}" VIDEO_EXPORT_SIZE)
    if(VIDEO_EXPORT_SIZE LESS 1000)
        message(FATAL_ERROR "video export output was unexpectedly small")
    endif()
    set(PLOT_VIDEO_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-plot-video.mp4")
    execute_process(
        COMMAND "${MARILA_EXECUTABLE}" export "${SOURCE_DIR}/examples/plot_video_demo.marila" mp4 "${PLOT_VIDEO_OUTPUT}"
        RESULT_VARIABLE PLOT_VIDEO_RESULT
        ERROR_VARIABLE PLOT_VIDEO_ERROR
    )
    if(PLOT_VIDEO_RESULT)
        message(FATAL_ERROR "plot video export failed: ${PLOT_VIDEO_ERROR}")
    endif()
    file(SIZE "${PLOT_VIDEO_OUTPUT}" PLOT_VIDEO_SIZE)
    if(PLOT_VIDEO_SIZE LESS 1000)
        message(FATAL_ERROR "plot video export output was unexpectedly small")
    endif()
    set(PLOT_VIDEO_HTML_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-plot-video.html")
    execute_process(
        COMMAND "${MARILA_EXECUTABLE}" render "${SOURCE_DIR}/examples/plot_video_demo.marila" "${PLOT_VIDEO_HTML_OUTPUT}"
        RESULT_VARIABLE PLOT_VIDEO_HTML_RESULT
        ERROR_VARIABLE PLOT_VIDEO_HTML_ERROR
    )
    if(PLOT_VIDEO_HTML_RESULT)
        message(FATAL_ERROR "plot video HTML render failed: ${PLOT_VIDEO_HTML_ERROR}")
    endif()
    file(READ "${PLOT_VIDEO_HTML_OUTPUT}" PLOT_VIDEO_HTML_TEXT)
    if(NOT PLOT_VIDEO_HTML_TEXT MATCHES "trace:true" OR NOT PLOT_VIDEO_HTML_TEXT MATCHES "duration:3" OR NOT PLOT_VIDEO_HTML_TEXT MATCHES "duration:2" OR NOT PLOT_VIDEO_HTML_TEXT MATCHES "start:1" OR NOT PLOT_VIDEO_HTML_TEXT MATCHES "start:0.5" OR NOT PLOT_VIDEO_HTML_TEXT MATCHES "repeat:2" OR NOT PLOT_VIDEO_HTML_TEXT MATCHES "yoyo:true" OR NOT PLOT_VIDEO_HTML_TEXT MATCHES "easing:\"in-out\"" OR NOT PLOT_VIDEO_HTML_TEXT MATCHES "type:'data2d'")
        message(FATAL_ERROR "interactive plot trace did not preserve timeline semantics")
    endif()
    set(SURFACE_VIDEO_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-surface-video.mp4")
    execute_process(
        COMMAND "${MARILA_EXECUTABLE}" export "${SOURCE_DIR}/examples/surface_video_demo.marila" mp4 "${SURFACE_VIDEO_OUTPUT}"
        RESULT_VARIABLE SURFACE_VIDEO_RESULT
        ERROR_VARIABLE SURFACE_VIDEO_ERROR
    )
    if(SURFACE_VIDEO_RESULT)
        message(FATAL_ERROR "surface video export failed: ${SURFACE_VIDEO_ERROR}")
    endif()
    file(SIZE "${SURFACE_VIDEO_OUTPUT}" SURFACE_VIDEO_SIZE)
    if(SURFACE_VIDEO_SIZE LESS 1000)
        message(FATAL_ERROR "surface video export output was unexpectedly small")
    endif()
    set(SURFACE_VIDEO_HTML_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-surface-video.html")
    execute_process(
        COMMAND "${MARILA_EXECUTABLE}" render "${SOURCE_DIR}/examples/surface_video_demo.marila" "${SURFACE_VIDEO_HTML_OUTPUT}"
        RESULT_VARIABLE SURFACE_VIDEO_HTML_RESULT
        ERROR_VARIABLE SURFACE_VIDEO_HTML_ERROR
    )
    if(SURFACE_VIDEO_HTML_RESULT)
        message(FATAL_ERROR "surface video HTML render failed: ${SURFACE_VIDEO_HTML_ERROR}")
    endif()
    file(READ "${SURFACE_VIDEO_HTML_OUTPUT}" SURFACE_VIDEO_HTML_TEXT)
    if(NOT SURFACE_VIDEO_HTML_TEXT MATCHES "type:'3d'" OR NOT SURFACE_VIDEO_HTML_TEXT MATCHES "trace:true" OR NOT SURFACE_VIDEO_HTML_TEXT MATCHES "duration:2" OR NOT SURFACE_VIDEO_HTML_TEXT MATCHES "start:0.5" OR NOT SURFACE_VIDEO_HTML_TEXT MATCHES "repeat:2" OR NOT SURFACE_VIDEO_HTML_TEXT MATCHES "yoyo:true")
        message(FATAL_ERROR "3D surface trace did not preserve timeline semantics")
    endif()
endif()

set(DOCUMENT_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-document.html")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" render "${SOURCE_DIR}/examples/document_demo.marila" "${DOCUMENT_OUTPUT}"
    RESULT_VARIABLE DOCUMENT_RESULT
    ERROR_VARIABLE DOCUMENT_ERROR
)
if(DOCUMENT_RESULT)
    message(FATAL_ERROR "document render failed: ${DOCUMENT_ERROR}")
endif()
file(READ "${DOCUMENT_OUTPUT}" DOCUMENT_TEXT)
if(NOT DOCUMENT_TEXT MATCHES "<figure>")
    message(FATAL_ERROR "document output did not embed its visual nodes")
endif()
if(NOT DOCUMENT_TEXT MATCHES "<h2" OR NOT DOCUMENT_TEXT MATCHES "<blockquote>" OR NOT DOCUMENT_TEXT MATCHES "<a href=\"https://example.com/marila\">")
    message(FATAL_ERROR "document output did not preserve semantic headings, quotation, or link")
endif()

set(EXPORT_HTML_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-export.html")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" export "${SOURCE_DIR}/examples/document_demo.marila" html "${EXPORT_HTML_OUTPUT}"
    RESULT_VARIABLE EXPORT_HTML_RESULT
    ERROR_VARIABLE EXPORT_HTML_ERROR
)
if(EXPORT_HTML_RESULT)
    message(FATAL_ERROR "HTML export failed: ${EXPORT_HTML_ERROR}")
endif()
file(READ "${EXPORT_HTML_OUTPUT}" EXPORT_HTML_TEXT)
if(NOT EXPORT_HTML_TEXT MATCHES "<!doctype html>" OR NOT EXPORT_HTML_TEXT MATCHES "<figure>")
    message(FATAL_ERROR "HTML export did not contain the document output")
endif()

set(EXPORT_SVG_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-export.svg")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" export "${SOURCE_DIR}/examples/scene_geometry_demo.marila" svg "${EXPORT_SVG_OUTPUT}"
    RESULT_VARIABLE EXPORT_SVG_RESULT
    ERROR_VARIABLE EXPORT_SVG_ERROR
)
if(EXPORT_SVG_RESULT)
    message(FATAL_ERROR "SVG export failed: ${EXPORT_SVG_ERROR}")
endif()
file(READ "${EXPORT_SVG_OUTPUT}" EXPORT_SVG_TEXT)
if(NOT EXPORT_SVG_TEXT MATCHES "<svg" OR NOT EXPORT_SVG_TEXT MATCHES "data-solid=\"sphere\"")
    message(FATAL_ERROR "SVG export did not contain static scene geometry")
endif()

set(EXPORT_4D_SVG_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-export-4d.svg")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" export "${SOURCE_DIR}/examples/static_4d_image_demo.marila" svg "${EXPORT_4D_SVG_OUTPUT}"
    RESULT_VARIABLE EXPORT_4D_SVG_RESULT
    ERROR_VARIABLE EXPORT_4D_SVG_ERROR
)
if(EXPORT_4D_SVG_RESULT)
    message(FATAL_ERROR "4D SVG export failed: ${EXPORT_4D_SVG_ERROR}")
endif()
file(READ "${EXPORT_4D_SVG_OUTPUT}" EXPORT_4D_SVG_TEXT)
if(NOT EXPORT_4D_SVG_TEXT MATCHES "data-plot=\"4d\"" OR NOT EXPORT_4D_SVG_TEXT MATCHES "Projected 4D curve")
    message(FATAL_ERROR "4D SVG export did not preserve the projected curve")
endif()

find_program(RSVG_CONVERT_EXPORT_EXECUTABLE rsvg-convert)
if(RSVG_CONVERT_EXPORT_EXECUTABLE)
    set(EXPORT_PNG_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-export.png")
    execute_process(
        COMMAND "${MARILA_EXECUTABLE}" export "${SOURCE_DIR}/examples/scene_geometry_demo.marila" png "${EXPORT_PNG_OUTPUT}"
        RESULT_VARIABLE EXPORT_PNG_RESULT
        ERROR_VARIABLE EXPORT_PNG_ERROR
    )
    if(EXPORT_PNG_RESULT)
        message(FATAL_ERROR "PNG export failed: ${EXPORT_PNG_ERROR}")
    endif()
    file(SIZE "${EXPORT_PNG_OUTPUT}" EXPORT_PNG_SIZE)
    if(EXPORT_PNG_SIZE LESS 1000)
        message(FATAL_ERROR "PNG export output was unexpectedly small")
    endif()
endif()

set(EXPORT_TEX_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-export.tex")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" export "${SOURCE_DIR}/examples/latex_demo.marila" tex "${EXPORT_TEX_OUTPUT}"
    RESULT_VARIABLE EXPORT_TEX_RESULT
    ERROR_VARIABLE EXPORT_TEX_ERROR
)
if(EXPORT_TEX_RESULT)
    message(FATAL_ERROR "TeX export failed: ${EXPORT_TEX_ERROR}")
endif()
file(READ "${EXPORT_TEX_OUTPUT}" EXPORT_TEX_TEXT)
if(NOT EXPORT_TEX_TEXT MATCHES "\\\\documentclass" OR NOT EXPORT_TEX_TEXT MATCHES "\\\\begin\\{document\\}")
    message(FATAL_ERROR "TeX export did not contain a compilable document")
endif()

set(EXPORT_TIKZ_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-export-tikz.tex")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" export "${SOURCE_DIR}/examples/tikz_demo.marila" tikz "${EXPORT_TIKZ_OUTPUT}"
    RESULT_VARIABLE EXPORT_TIKZ_RESULT
    ERROR_VARIABLE EXPORT_TIKZ_ERROR
)
if(EXPORT_TIKZ_RESULT)
    message(FATAL_ERROR "TikZ export failed: ${EXPORT_TIKZ_ERROR}")
endif()
file(READ "${EXPORT_TIKZ_OUTPUT}" EXPORT_TIKZ_TEXT)
if(NOT EXPORT_TIKZ_TEXT MATCHES "\\\\usepackage\\{tikz\\}" OR NOT EXPORT_TIKZ_TEXT MATCHES "\\\\draw")
    message(FATAL_ERROR "TikZ export did not contain geometry")
endif()

set(EXPORT_SCENE_HTML_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-export-scene.html")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" export "${SOURCE_DIR}/examples/motion_demo.marila" html "${EXPORT_SCENE_HTML_OUTPUT}"
    RESULT_VARIABLE EXPORT_SCENE_HTML_RESULT
    ERROR_VARIABLE EXPORT_SCENE_HTML_ERROR
)
if(EXPORT_SCENE_HTML_RESULT)
    message(FATAL_ERROR "scene HTML export failed: ${EXPORT_SCENE_HTML_ERROR}")
endif()
file(READ "${EXPORT_SCENE_HTML_OUTPUT}" EXPORT_SCENE_HTML_TEXT)
if(NOT EXPORT_SCENE_HTML_TEXT MATCHES "id=\"timeline\"" OR NOT EXPORT_SCENE_HTML_TEXT MATCHES "type:'move'")
    message(FATAL_ERROR "scene HTML export did not preserve interactive animation data")
endif()

find_program(RSVG_CONVERT_EXECUTABLE rsvg-convert)
if(RSVG_CONVERT_EXECUTABLE)
    set(EXPORT_PDF_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-export.pdf")
    execute_process(
        COMMAND "${MARILA_EXECUTABLE}" export "${SOURCE_DIR}/examples/plot_image_demo.marila" pdf "${EXPORT_PDF_OUTPUT}"
        RESULT_VARIABLE EXPORT_PDF_RESULT
        ERROR_VARIABLE EXPORT_PDF_ERROR
    )
    if(EXPORT_PDF_RESULT)
        message(FATAL_ERROR "PDF export failed: ${EXPORT_PDF_ERROR}")
    endif()
    file(SIZE "${EXPORT_PDF_OUTPUT}" EXPORT_PDF_SIZE)
    if(EXPORT_PDF_SIZE LESS 1000)
        message(FATAL_ERROR "PDF export output was unexpectedly small")
    endif()
endif()
if(NOT DOCUMENT_TEXT MATCHES "<path")
    message(FATAL_ERROR "document output did not embed its plot path")
endif()
if(NOT DOCUMENT_TEXT MATCHES "<li>")
    message(FATAL_ERROR "document output did not render bullet items")
endif()
if(NOT DOCUMENT_TEXT MATCHES "<ul><li>Points, lines, and curves share one visual model.</li><li>The same source can become HTML, LaTeX, or SVG.</li></ul>")
    message(FATAL_ERROR "document output did not group consecutive bullet items")
endif()
if(NOT DOCUMENT_TEXT MATCHES "<ol>" OR NOT DOCUMENT_TEXT MATCHES "First render")
    message(FATAL_ERROR "document output did not render ordered items")
endif()
if(NOT DOCUMENT_TEXT MATCHES "<pre><code>")
    message(FATAL_ERROR "document output did not render code blocks")
endif()
if(NOT DOCUMENT_TEXT MATCHES "class=\"document-table\"")
    message(FATAL_ERROR "document output did not render a table")
endif()
if(NOT DOCUMENT_TEXT MATCHES "<th>Quantity</th>")
    message(FATAL_ERROR "document table did not preserve its header")
endif()

set(DOCUMENT_ESCAPE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-document-escape.html")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" render "${SOURCE_DIR}/examples/document_escape_demo.marila" "${DOCUMENT_ESCAPE_OUTPUT}"
    RESULT_VARIABLE DOCUMENT_ESCAPE_RESULT
    ERROR_VARIABLE DOCUMENT_ESCAPE_ERROR
)
if(DOCUMENT_ESCAPE_RESULT)
    message(FATAL_ERROR "escaped document render failed: ${DOCUMENT_ESCAPE_ERROR}")
endif()
file(READ "${DOCUMENT_ESCAPE_OUTPUT}" DOCUMENT_ESCAPE_TEXT)
if(NOT DOCUMENT_ESCAPE_TEXT MATCHES "&quot;quoted&quot;" OR NOT DOCUMENT_ESCAPE_TEXT MATCHES "Author\\\\Name")
    message(FATAL_ERROR "document renderer did not decode escaped text: ${DOCUMENT_ESCAPE_TEXT}")
endif()

set(VIDEO_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-video.html")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" render "${SOURCE_DIR}/examples/video_demo.marila" "${VIDEO_OUTPUT}"
    RESULT_VARIABLE VIDEO_RESULT
    ERROR_VARIABLE VIDEO_ERROR
)
if(VIDEO_RESULT)
    message(FATAL_ERROR "video render failed: ${VIDEO_ERROR}")
endif()
file(READ "${VIDEO_OUTPUT}" VIDEO_TEXT)
if(NOT VIDEO_TEXT MATCHES "duration:1")
    message(FATAL_ERROR "video output did not preserve one-second duration")
endif()
if(NOT VIDEO_TEXT MATCHES "easing:\"out\"")
    message(FATAL_ERROR "video output did not preserve morph easing mode")
endif()
if(NOT VIDEO_TEXT MATCHES "id=\"timeline\"" OR NOT VIDEO_TEXT MATCHES "id=\"play\"")
    message(FATAL_ERROR "video output did not contain animation controls")
endif()
set(PLOT_KINDS_VIDEO_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-plot-kinds.mp4")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" export "${SOURCE_DIR}/examples/plot_kinds_video_demo.marila" mp4 "${PLOT_KINDS_VIDEO_OUTPUT}"
    RESULT_VARIABLE PLOT_KINDS_VIDEO_RESULT
    ERROR_VARIABLE PLOT_KINDS_VIDEO_ERROR
)
if(PLOT_KINDS_VIDEO_RESULT)
    message(FATAL_ERROR "plot-kind video export failed: ${PLOT_KINDS_VIDEO_ERROR}")
endif()
file(SIZE "${PLOT_KINDS_VIDEO_OUTPUT}" PLOT_KINDS_VIDEO_SIZE)
if(PLOT_KINDS_VIDEO_SIZE LESS 1000)
    message(FATAL_ERROR "plot-kind video output was unexpectedly small")
endif()
    set(PLOT_KINDS_VIDEO_HTML_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-plot-kinds.html")
    execute_process(
        COMMAND "${MARILA_EXECUTABLE}" render "${SOURCE_DIR}/examples/plot_kinds_video_demo.marila" "${PLOT_KINDS_VIDEO_HTML_OUTPUT}"
        RESULT_VARIABLE PLOT_KINDS_VIDEO_HTML_RESULT
        ERROR_VARIABLE PLOT_KINDS_VIDEO_HTML_ERROR
    )
    if(PLOT_KINDS_VIDEO_HTML_RESULT)
        message(FATAL_ERROR "plot-kind HTML render failed: ${PLOT_KINDS_VIDEO_HTML_ERROR}")
    endif()
    file(READ "${PLOT_KINDS_VIDEO_HTML_OUTPUT}" PLOT_KINDS_VIDEO_HTML_TEXT)
    if(NOT PLOT_KINDS_VIDEO_HTML_TEXT MATCHES "type:'param4d'" OR NOT PLOT_KINDS_VIDEO_HTML_TEXT MATCHES "trace:true" OR NOT PLOT_KINDS_VIDEO_HTML_TEXT MATCHES "start:0.25" OR NOT PLOT_KINDS_VIDEO_HTML_TEXT MATCHES "repeat:2" OR NOT PLOT_KINDS_VIDEO_HTML_TEXT MATCHES "yoyo:true")
        message(FATAL_ERROR "4D trace did not preserve timeline semantics")
    endif()
set(MOTION_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-motion.html")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" render "${SOURCE_DIR}/examples/motion_demo.marila" "${MOTION_OUTPUT}"
    RESULT_VARIABLE MOTION_RESULT
    ERROR_VARIABLE MOTION_ERROR
)
if(MOTION_RESULT)
    message(FATAL_ERROR "motion render failed: ${MOTION_ERROR}")
endif()
file(READ "${MOTION_OUTPUT}" MOTION_TEXT)
if(NOT MOTION_TEXT MATCHES "type:'move'" OR NOT MOTION_TEXT MATCHES "duration:2")
    message(FATAL_ERROR "motion output did not preserve point movement")
endif()
if(NOT MOTION_TEXT MATCHES "easing:\"in-out\"")
    message(FATAL_ERROR "motion output did not preserve easing mode")
endif()
if(NOT MOTION_TEXT MATCHES "repeat:2" OR NOT MOTION_TEXT MATCHES "yoyo:true")
    message(FATAL_ERROR "motion output did not preserve repeat/yoyo semantics")
endif()
if(NOT MOTION_TEXT MATCHES "start:5" OR NOT MOTION_TEXT MATCHES "tx:0")
    message(FATAL_ERROR "motion output did not preserve sequential move segments")
endif()
if(NOT MOTION_TEXT MATCHES "start:1")
    message(FATAL_ERROR "motion output did not preserve animation start time")
endif()
if(NOT MOTION_TEXT MATCHES "type:'fade'.*target:\"A\".*fadeIn:true")
    message(FATAL_ERROR "motion output did not preserve fade animation")
endif()
if(NOT MOTION_TEXT MATCHES "q.target===id&&t>=")
    message(FATAL_ERROR "motion output did not guard fades until their scheduled start")
endif()
if(NOT MOTION_TEXT MATCHES "type:'wait'.*duration:0.5.*start:3")
    message(FATAL_ERROR "motion output did not preserve wait interval")
endif()

set(RANGE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-ranges.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/ranges_demo.marila"
    RESULT_VARIABLE RANGE_RESULT
    OUTPUT_FILE "${RANGE_OUTPUT}"
    ERROR_VARIABLE RANGE_ERROR
)
if(RANGE_RESULT)
    message(FATAL_ERROR "range run failed: ${RANGE_ERROR}")
endif()
file(READ "${RANGE_OUTPUT}" RANGE_TEXT)
if(NOT RANGE_TEXT MATCHES "\\[1, 3, 5\\]")
    message(FATAL_ERROR "range output did not contain expected values: ${RANGE_TEXT}")
endif()
if(NOT RANGE_TEXT MATCHES "\\[0, 0.25, 0.5, 0.75\\]")
    message(FATAL_ERROR "floating-point range output was unexpected: ${RANGE_TEXT}")
endif()
if(NOT RANGE_TEXT MATCHES "\\[\\[1, 8\\], \\[3, 4\\]\\]")
    message(FATAL_ERROR "indexed assignment output was not preserved: ${RANGE_TEXT}")
endif()

set(SCIENTIFIC_PROGRAM_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-scientific-program.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/scientific_program_demo.marila"
    RESULT_VARIABLE SCIENTIFIC_PROGRAM_RESULT
    OUTPUT_FILE "${SCIENTIFIC_PROGRAM_OUTPUT}"
    ERROR_VARIABLE SCIENTIFIC_PROGRAM_ERROR
)
if(SCIENTIFIC_PROGRAM_RESULT)
    message(FATAL_ERROR "scientific program run failed: ${SCIENTIFIC_PROGRAM_ERROR}")
endif()
file(READ "${SCIENTIFIC_PROGRAM_OUTPUT}" SCIENTIFIC_PROGRAM_TEXT)
if(NOT SCIENTIFIC_PROGRAM_TEXT MATCHES "6.022e" OR NOT SCIENTIFIC_PROGRAM_TEXT MATCHES "0.00015" OR NOT SCIENTIFIC_PROGRAM_TEXT MATCHES "λ")
    message(FATAL_ERROR "scientific literals or Unicode escapes were decoded incorrectly: ${SCIENTIFIC_PROGRAM_TEXT}")
endif()

set(LINEAR_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-linear.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/linear_algebra_demo.marila"
    RESULT_VARIABLE LINEAR_RESULT
    OUTPUT_FILE "${LINEAR_OUTPUT}"
    ERROR_VARIABLE LINEAR_ERROR
)
if(LINEAR_RESULT)
    message(FATAL_ERROR "linear algebra run failed: ${LINEAR_ERROR}")
endif()
file(READ "${LINEAR_OUTPUT}" LINEAR_TEXT)
if(NOT LINEAR_TEXT MATCHES "^10")
    message(FATAL_ERROR "determinant output was not 10: ${LINEAR_TEXT}")
endif()
if(NOT LINEAR_TEXT MATCHES "1.8")
    message(FATAL_ERROR "solve output did not contain 1.8: ${LINEAR_TEXT}")
endif()
if(NOT LINEAR_TEXT MATCHES "10.*\\[\\[4, 0\\], \\[0, 6\\]\\].*\\[\\[3, 4\\], \\[6, 8\\]\\]")
    message(FATAL_ERROR "trace/diag output was not present: ${LINEAR_TEXT}")
endif()

set(CALCULUS_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-calculus.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/calculus_demo.marila"
    RESULT_VARIABLE CALCULUS_RESULT
    OUTPUT_FILE "${CALCULUS_OUTPUT}"
    ERROR_VARIABLE CALCULUS_ERROR
)
if(CALCULUS_RESULT)
    message(FATAL_ERROR "calculus run failed: ${CALCULUS_ERROR}")
endif()
file(READ "${CALCULUS_OUTPUT}" CALCULUS_TEXT)
if(NOT CALCULUS_TEXT MATCHES "^6")
    message(FATAL_ERROR "differentiate output was not approximately 6: ${CALCULUS_TEXT}")
endif()
if(NOT CALCULUS_TEXT MATCHES "0.333")
    message(FATAL_ERROR "integrate output was not approximately 1/3: ${CALCULUS_TEXT}")
endif()
if(NOT CALCULUS_TEXT MATCHES "5")
    message(FATAL_ERROR "interp1 output was not 5: ${CALCULUS_TEXT}")
endif()

set(SOLVERS_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-solvers.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/solvers_demo.marila"
    RESULT_VARIABLE SOLVERS_RESULT
    OUTPUT_FILE "${SOLVERS_OUTPUT}"
    ERROR_VARIABLE SOLVERS_ERROR
)
if(SOLVERS_RESULT)
    message(FATAL_ERROR "solver run failed: ${SOLVERS_ERROR}")
endif()
file(READ "${SOLVERS_OUTPUT}" SOLVERS_TEXT)
if(NOT SOLVERS_TEXT MATCHES "1.41421")
    message(FATAL_ERROR "root solver output was unexpected: ${SOLVERS_TEXT}")
endif()
if(NOT SOLVERS_TEXT MATCHES "2.4414")
    message(FATAL_ERROR "ODE solver output was unexpected: ${SOLVERS_TEXT}")
endif()
if(NOT SOLVERS_TEXT MATCHES "2.7182")
    message(FATAL_ERROR "RK4 solver output was unexpected: ${SOLVERS_TEXT}")
endif()

set(FOURIER_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-fourier.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/fourier_demo.marila"
    RESULT_VARIABLE FOURIER_RESULT
    OUTPUT_FILE "${FOURIER_OUTPUT}"
    ERROR_VARIABLE FOURIER_ERROR
)
if(FOURIER_RESULT)
    message(FATAL_ERROR "Fourier run failed: ${FOURIER_ERROR}")
endif()
file(READ "${FOURIER_OUTPUT}" FOURIER_TEXT)
if(NOT FOURIER_TEXT MATCHES "\\[1, 1, 1, 1\\]")
    message(FATAL_ERROR "FFT output was unexpected: ${FOURIER_TEXT}")
endif()
if(NOT FOURIER_TEXT MATCHES "\\[1, 0, 0, 0\\]")
    message(FATAL_ERROR "IFFT round trip was unexpected: ${FOURIER_TEXT}")
endif()

set(POLYNOMIAL_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-polynomial.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/polynomial_demo.marila"
    RESULT_VARIABLE POLYNOMIAL_RESULT
    OUTPUT_FILE "${POLYNOMIAL_OUTPUT}"
    ERROR_VARIABLE POLYNOMIAL_ERROR
)
if(POLYNOMIAL_RESULT)
    message(FATAL_ERROR "polynomial run failed: ${POLYNOMIAL_ERROR}")
endif()
file(READ "${POLYNOMIAL_OUTPUT}" POLYNOMIAL_TEXT)
if(NOT POLYNOMIAL_TEXT MATCHES "\\[2, 1\\]")
    message(FATAL_ERROR "polyfit output was unexpected: ${POLYNOMIAL_TEXT}")
endif()
if(NOT POLYNOMIAL_TEXT MATCHES "9")
    message(FATAL_ERROR "polyval output was unexpected: ${POLYNOMIAL_TEXT}")
endif()

set(MATRIX_GRADIENT_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-matrix-gradient.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/matrix_gradient_demo.marila"
    RESULT_VARIABLE MATRIX_GRADIENT_RESULT
    OUTPUT_FILE "${MATRIX_GRADIENT_OUTPUT}"
    ERROR_VARIABLE MATRIX_GRADIENT_ERROR
)
if(MATRIX_GRADIENT_RESULT)
    message(FATAL_ERROR "matrix gradient run failed: ${MATRIX_GRADIENT_ERROR}")
endif()
file(READ "${MATRIX_GRADIENT_OUTPUT}" MATRIX_GRADIENT_TEXT)
if(NOT MATRIX_GRADIENT_TEXT MATCHES "\\[\\[1, 1, 1\\], \\[1, 1, 1\\], \\[1, 1, 1\\]\\]")
    message(FATAL_ERROR "matrix gradient output was unexpected: ${MATRIX_GRADIENT_TEXT}")
endif()

set(EQUALITY_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-equality.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/equality_demo.marila"
    RESULT_VARIABLE EQUALITY_RESULT
    OUTPUT_FILE "${EQUALITY_OUTPUT}"
    ERROR_VARIABLE EQUALITY_ERROR
)
if(EQUALITY_RESULT)
    message(FATAL_ERROR "array equality run failed: ${EQUALITY_ERROR}")
endif()
file(READ "${EQUALITY_OUTPUT}" EQUALITY_TEXT)
if(NOT EQUALITY_TEXT MATCHES "true")
    message(FATAL_ERROR "structural array equality was not true: ${EQUALITY_TEXT}")
endif()

set(MODULE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-modules.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/modules_demo.marila"
    RESULT_VARIABLE MODULE_RESULT
    OUTPUT_FILE "${MODULE_OUTPUT}"
    ERROR_VARIABLE MODULE_ERROR
)
if(MODULE_RESULT)
    message(FATAL_ERROR "module run failed: ${MODULE_ERROR}")
endif()
file(READ "${MODULE_OUTPUT}" MODULE_TEXT)
if(NOT MODULE_TEXT MATCHES "25" OR NOT MODULE_TEXT MATCHES "3")
    message(FATAL_ERROR "module imports did not expose expected values: ${MODULE_TEXT}")
endif()

set(JSONL_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-jsonl.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/jsonl_demo.marila"
    RESULT_VARIABLE JSONL_RESULT
    OUTPUT_FILE "${JSONL_OUTPUT}"
    ERROR_VARIABLE JSONL_ERROR
)
if(JSONL_RESULT)
    message(FATAL_ERROR "JSON Lines run failed: ${JSONL_ERROR}")
endif()
file(READ "${JSONL_OUTPUT}" JSONL_TEXT)
if(NOT JSONL_TEXT MATCHES "12")
    message(FATAL_ERROR "JSON Lines round-trip was unexpected: ${JSONL_TEXT}")
endif()

set(VECTOR_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-vector.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/vector_demo.marila"
    RESULT_VARIABLE VECTOR_RESULT
    OUTPUT_FILE "${VECTOR_OUTPUT}"
    ERROR_VARIABLE VECTOR_ERROR
)
if(VECTOR_RESULT)
    message(FATAL_ERROR "vector run failed: ${VECTOR_ERROR}")
endif()
file(READ "${VECTOR_OUTPUT}" VECTOR_TEXT)
if(NOT VECTOR_TEXT MATCHES "5")
    message(FATAL_ERROR "norm output was not 5: ${VECTOR_TEXT}")
endif()
if(NOT VECTOR_TEXT MATCHES "\\[0.6, 0.8\\]")
    message(FATAL_ERROR "normalize output was not expected: ${VECTOR_TEXT}")
endif()
if(NOT VECTOR_TEXT MATCHES "\\[0, 0, 1\\]")
    message(FATAL_ERROR "cross output was not expected: ${VECTOR_TEXT}")
endif()
if(NOT VECTOR_TEXT MATCHES "\\[\\[1\\], \\[2\\], \\[3\\]\\]")
    message(FATAL_ERROR "vector transpose output was not expected: ${VECTOR_TEXT}")
endif()

set(MATRIX_NORM_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-matrix-norm.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/matrix_norm_demo.marila"
    RESULT_VARIABLE MATRIX_NORM_RESULT
    OUTPUT_FILE "${MATRIX_NORM_OUTPUT}"
    ERROR_VARIABLE MATRIX_NORM_ERROR
)
if(MATRIX_NORM_RESULT)
    message(FATAL_ERROR "matrix norm run failed: ${MATRIX_NORM_ERROR}")
endif()
file(READ "${MATRIX_NORM_OUTPUT}" MATRIX_NORM_TEXT)
if(NOT MATRIX_NORM_TEXT MATCHES "5.11667.*6")
    message(FATAL_ERROR "matrix norm output was unexpected: ${MATRIX_NORM_TEXT}")
endif()

set(RANDOM_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-random.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/random_demo.marila"
    RESULT_VARIABLE RANDOM_RESULT
    OUTPUT_FILE "${RANDOM_OUTPUT}"
    ERROR_VARIABLE RANDOM_ERROR
)
if(RANDOM_RESULT)
    message(FATAL_ERROR "random generator run failed: ${RANDOM_ERROR}")
endif()
file(READ "${RANDOM_OUTPUT}" RANDOM_TEXT)
if(NOT RANDOM_TEXT MATCHES "true" OR NOT RANDOM_TEXT MATCHES "\\[3, 3\\]")
    message(FATAL_ERROR "random generator output was unexpected: ${RANDOM_TEXT}")
endif()

set(CONDITIONAL_EXPRESSION_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-conditional-expression.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/conditional_expression_demo.marila"
    RESULT_VARIABLE CONDITIONAL_EXPRESSION_RESULT
    OUTPUT_FILE "${CONDITIONAL_EXPRESSION_OUTPUT}"
    ERROR_VARIABLE CONDITIONAL_EXPRESSION_ERROR
)
if(CONDITIONAL_EXPRESSION_RESULT)
    message(FATAL_ERROR "conditional expression run failed: ${CONDITIONAL_EXPRESSION_ERROR}")
endif()
file(READ "${CONDITIONAL_EXPRESSION_OUTPUT}" CONDITIONAL_EXPRESSION_TEXT)
if(NOT CONDITIONAL_EXPRESSION_TEXT MATCHES "negative" OR NOT CONDITIONAL_EXPRESSION_TEXT MATCHES "4")
    message(FATAL_ERROR "conditional expression output was unexpected: ${CONDITIONAL_EXPRESSION_TEXT}")
endif()

set(DIAG_OFFSET_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-diag-offset.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/diag_offset_demo.marila"
    RESULT_VARIABLE DIAG_OFFSET_RESULT
    OUTPUT_FILE "${DIAG_OFFSET_OUTPUT}"
    ERROR_VARIABLE DIAG_OFFSET_ERROR
)
if(DIAG_OFFSET_RESULT)
    message(FATAL_ERROR "diagonal offset run failed: ${DIAG_OFFSET_ERROR}")
endif()
file(READ "${DIAG_OFFSET_OUTPUT}" DIAG_OFFSET_TEXT)
if(NOT DIAG_OFFSET_TEXT MATCHES "\\[\\[0, 1, 0, 0\\], \\[0, 0, 2, 0\\], \\[0, 0, 0, 3\\], \\[0, 0, 0, 0\\]\\]")
    message(FATAL_ERROR "diagonal construction output was unexpected: ${DIAG_OFFSET_TEXT}")
endif()
if(NOT DIAG_OFFSET_TEXT MATCHES "\\[\\[0, 0, 0\\], \\[1, 0, 0\\], \\[0, 2, 0\\]\\]")
    message(FATAL_ERROR "subdiagonal construction output was unexpected: ${DIAG_OFFSET_TEXT}")
endif()
if(NOT DIAG_OFFSET_TEXT MATCHES "\\[4\\]")
    message(FATAL_ERROR "diagonal extraction output was unexpected: ${DIAG_OFFSET_TEXT}")
endif()

set(GENERAL_EIGEN_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-general-eigen.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/general_eigen_demo.marila"
    RESULT_VARIABLE GENERAL_EIGEN_RESULT
    OUTPUT_FILE "${GENERAL_EIGEN_OUTPUT}"
    ERROR_VARIABLE GENERAL_EIGEN_ERROR
)
if(GENERAL_EIGEN_RESULT)
    message(FATAL_ERROR "general eigenvalue run failed: ${GENERAL_EIGEN_ERROR}")
endif()
file(READ "${GENERAL_EIGEN_OUTPUT}" GENERAL_EIGEN_TEXT)
if(NOT GENERAL_EIGEN_TEXT MATCHES "1i" OR NOT GENERAL_EIGEN_TEXT MATCHES "- 1i")
    message(FATAL_ERROR "general eigenvalue output was unexpected: ${GENERAL_EIGEN_TEXT}")
endif()

set(MULTIPLE_RHS_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-multiple-rhs.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/multiple_rhs_demo.marila"
    RESULT_VARIABLE MULTIPLE_RHS_RESULT
    OUTPUT_FILE "${MULTIPLE_RHS_OUTPUT}"
    ERROR_VARIABLE MULTIPLE_RHS_ERROR
)
if(MULTIPLE_RHS_RESULT)
    message(FATAL_ERROR "multiple right-hand-side solve failed: ${MULTIPLE_RHS_ERROR}")
endif()
file(READ "${MULTIPLE_RHS_OUTPUT}" MULTIPLE_RHS_TEXT)
if(NOT MULTIPLE_RHS_TEXT MATCHES "\\[\\[2, 0.6\\], \\[3, 2.2\\]\\]")
    message(FATAL_ERROR "multiple right-hand-side solve output was unexpected: ${MULTIPLE_RHS_TEXT}")
endif()

set(SCIENTIFIC_SCENE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-scientific-scene.json")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" ast "${SOURCE_DIR}/examples/scientific_scene_demo.marila"
    RESULT_VARIABLE SCIENTIFIC_SCENE_RESULT
    OUTPUT_FILE "${SCIENTIFIC_SCENE_OUTPUT}"
    ERROR_VARIABLE SCIENTIFIC_SCENE_ERROR
)
if(SCIENTIFIC_SCENE_RESULT)
    message(FATAL_ERROR "scientific visual literal parsing failed: ${SCIENTIFIC_SCENE_ERROR}")
endif()
file(READ "${SCIENTIFIC_SCENE_OUTPUT}" SCIENTIFIC_SCENE_TEXT)
if(NOT SCIENTIFIC_SCENE_TEXT MATCHES "0.001" OR NOT SCIENTIFIC_SCENE_TEXT MATCHES "0.5" OR NOT SCIENTIFIC_SCENE_TEXT MATCHES "scientific")
    message(FATAL_ERROR "scientific visual literals were not preserved: ${SCIENTIFIC_SCENE_TEXT}")
endif()
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" lint "${SOURCE_DIR}/examples/scientific_scene_demo.marila"
    RESULT_VARIABLE SCIENTIFIC_SCENE_LINT_RESULT
    OUTPUT_VARIABLE SCIENTIFIC_SCENE_LINT_OUTPUT
    ERROR_VARIABLE SCIENTIFIC_SCENE_LINT_ERROR
)
if(SCIENTIFIC_SCENE_LINT_RESULT OR SCIENTIFIC_SCENE_LINT_ERROR MATCHES "no plot or animation primitive")
    message(FATAL_ERROR "geometry-only scene lint was unexpected: ${SCIENTIFIC_SCENE_LINT_OUTPUT}${SCIENTIFIC_SCENE_LINT_ERROR}")
endif()

set(SCIENTIFIC_DATA_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-scientific-data.json")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" ast "${SOURCE_DIR}/examples/scientific_data_demo.marila"
    RESULT_VARIABLE SCIENTIFIC_DATA_RESULT
    OUTPUT_FILE "${SCIENTIFIC_DATA_OUTPUT}"
    ERROR_VARIABLE SCIENTIFIC_DATA_ERROR
)
if(SCIENTIFIC_DATA_RESULT)
    message(FATAL_ERROR "scientific sampled-data parsing failed: ${SCIENTIFIC_DATA_ERROR}")
endif()
file(READ "${SCIENTIFIC_DATA_OUTPUT}" SCIENTIFIC_DATA_TEXT)
if(NOT SCIENTIFIC_DATA_TEXT MATCHES "0.001" OR NOT SCIENTIFIC_DATA_TEXT MATCHES "0.25" OR NOT SCIENTIFIC_DATA_TEXT MATCHES "scientific samples")
    message(FATAL_ERROR "scientific sampled data was not preserved: ${SCIENTIFIC_DATA_TEXT}")
endif()
set(SCIENTIFIC_DATA_SVG "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-scientific-data.svg")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" export "${SOURCE_DIR}/examples/scientific_data_demo.marila" svg "${SCIENTIFIC_DATA_SVG}"
    RESULT_VARIABLE SCIENTIFIC_DATA_SVG_RESULT
    ERROR_VARIABLE SCIENTIFIC_DATA_SVG_ERROR
)
if(SCIENTIFIC_DATA_SVG_RESULT)
    message(FATAL_ERROR "scientific expression SVG export failed: ${SCIENTIFIC_DATA_SVG_ERROR}")
endif()
file(READ "${SCIENTIFIC_DATA_SVG}" SCIENTIFIC_DATA_SVG_TEXT)
if(NOT SCIENTIFIC_DATA_SVG_TEXT MATCHES "data-plot=\"2d\"")
    message(FATAL_ERROR "scientific expression SVG did not contain a plot")
endif()

set(MATRIX_CONSTRUCTOR_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-matrix-constructors.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/matrix_constructor_demo.marila"
    RESULT_VARIABLE MATRIX_CONSTRUCTOR_RESULT
    OUTPUT_FILE "${MATRIX_CONSTRUCTOR_OUTPUT}"
    ERROR_VARIABLE MATRIX_CONSTRUCTOR_ERROR
)
if(MATRIX_CONSTRUCTOR_RESULT)
    message(FATAL_ERROR "matrix constructor run failed: ${MATRIX_CONSTRUCTOR_ERROR}")
endif()
file(READ "${MATRIX_CONSTRUCTOR_OUTPUT}" MATRIX_CONSTRUCTOR_TEXT)
if(NOT MATRIX_CONSTRUCTOR_TEXT MATCHES "\\[2, 2\\].*\\[2, 3\\]")
    message(FATAL_ERROR "matrix constructor shapes were unexpected: ${MATRIX_CONSTRUCTOR_TEXT}")
endif()

set(MATH_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-math.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/math_functions_demo.marila"
    RESULT_VARIABLE MATH_RESULT
    OUTPUT_FILE "${MATH_OUTPUT}"
    ERROR_VARIABLE MATH_ERROR
)
if(MATH_RESULT)
    message(FATAL_ERROR "math function run failed: ${MATH_ERROR}")
endif()
file(READ "${MATH_OUTPUT}" MATH_TEXT)
if(NOT MATH_TEXT MATCHES "5.*3.*4.*3.*1.5708")
    message(FATAL_ERROR "math function output was unexpected: ${MATH_TEXT}")
endif()

set(DECOMPOSITIONS_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-decompositions.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/decompositions_demo.marila"
    RESULT_VARIABLE DECOMPOSITIONS_RESULT
    OUTPUT_FILE "${DECOMPOSITIONS_OUTPUT}"
    ERROR_VARIABLE DECOMPOSITIONS_ERROR
)
if(DECOMPOSITIONS_RESULT)
    message(FATAL_ERROR "matrix decomposition run failed: ${DECOMPOSITIONS_ERROR}")
endif()
file(READ "${DECOMPOSITIONS_OUTPUT}" DECOMPOSITIONS_TEXT)
if(NOT DECOMPOSITIONS_TEXT MATCHES "\\[\\[0.57735, 0.707107\\], \\[0.57735, -0.707107\\], \\[0.57735, 0\\]\\]")
    message(FATAL_ERROR "QR output was unexpected: ${DECOMPOSITIONS_TEXT}")
endif()
if(NOT DECOMPOSITIONS_TEXT MATCHES "\\[\\[2, 0\\], \\[1, 1\\]\\]")
    message(FATAL_ERROR "Cholesky output was unexpected: ${DECOMPOSITIONS_TEXT}")
endif()

set(MATMUL_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-matmul.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/matmul_demo.marila"
    RESULT_VARIABLE MATMUL_RESULT
    OUTPUT_FILE "${MATMUL_OUTPUT}"
    ERROR_VARIABLE MATMUL_ERROR
)
if(MATMUL_RESULT)
    message(FATAL_ERROR "matrix multiplication run failed: ${MATMUL_ERROR}")
endif()
file(READ "${MATMUL_OUTPUT}" MATMUL_TEXT)
if(NOT MATMUL_TEXT MATCHES "\\[\\[58, 64\\], \\[139, 154\\]\\].*\\[\\[58, 64\\], \\[139, 154\\]\\].*32.*\\[17, 39\\].*\\[23, 34\\]")
    message(FATAL_ERROR "matrix multiplication output was unexpected: ${MATMUL_TEXT}")
endif()

set(MATLAB_MATRIX_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-matlab-matrix.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/matlab_matrix_demo.marila"
    RESULT_VARIABLE MATLAB_MATRIX_RESULT
    OUTPUT_FILE "${MATLAB_MATRIX_OUTPUT}"
    ERROR_VARIABLE MATLAB_MATRIX_ERROR
)
if(MATLAB_MATRIX_RESULT)
    message(FATAL_ERROR "MATLAB matrix literal run failed: ${MATLAB_MATRIX_ERROR}")
endif()
file(READ "${MATLAB_MATRIX_OUTPUT}" MATLAB_MATRIX_TEXT)
if(NOT MATLAB_MATRIX_TEXT MATCHES "\\[\\[1, 2\\], \\[3, 4\\]\\].*\\[17, 39\\].*1")
    message(FATAL_ERROR "MATLAB matrix literal output was unexpected: ${MATLAB_MATRIX_TEXT}")
endif()

set(STATS_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-statistics.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/statistics_demo.marila"
    RESULT_VARIABLE STATS_RESULT
    OUTPUT_FILE "${STATS_OUTPUT}"
    ERROR_VARIABLE STATS_ERROR
)
if(STATS_RESULT)
    message(FATAL_ERROR "statistics run failed: ${STATS_ERROR}")
endif()
file(READ "${STATS_OUTPUT}" STATS_TEXT)
if(NOT STATS_TEXT MATCHES "3.*5.25.*2.29129.*\\[1, 2, 4, 7\\].*\\[4, 5, 12, 14\\].*\\[1, 3\\].*1.*2")
    message(FATAL_ERROR "statistics output was unexpected: ${STATS_TEXT}")
endif()

set(TEXT_CSV_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-text-csv.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/text_csv_demo.marila"
    RESULT_VARIABLE TEXT_CSV_RESULT
    OUTPUT_FILE "${TEXT_CSV_OUTPUT}"
    ERROR_VARIABLE TEXT_CSV_ERROR
)
if(TEXT_CSV_RESULT)
    message(FATAL_ERROR "text CSV run failed: ${TEXT_CSV_ERROR}")
endif()
file(READ "${TEXT_CSV_OUTPUT}" TEXT_CSV_TEXT)
if(NOT TEXT_CSV_TEXT MATCHES "Ada.*likes, math.*Grace.*quoted")
    message(FATAL_ERROR "text CSV output was unexpected: ${TEXT_CSV_TEXT}")
endif()

set(JSON_OBJECT_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-json-object.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/json_object_demo.marila"
    RESULT_VARIABLE JSON_OBJECT_RESULT
    OUTPUT_FILE "${JSON_OBJECT_OUTPUT}"
    ERROR_VARIABLE JSON_OBJECT_ERROR
)
if(JSON_OBJECT_RESULT)
    message(FATAL_ERROR "JSON object run failed: ${JSON_OBJECT_ERROR}")
endif()
file(READ "${JSON_OBJECT_OUTPUT}" JSON_OBJECT_TEXT)
if(NOT JSON_OBJECT_TEXT MATCHES "Marila.*3.*true.*\\[kind, name, stats\\].*true")
    message(FATAL_ERROR "JSON object output was unexpected: ${JSON_OBJECT_TEXT}")
endif()

set(JSON_MUTATION_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-json-mutation.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/json_mutation_demo.marila"
    RESULT_VARIABLE JSON_MUTATION_RESULT
    OUTPUT_FILE "${JSON_MUTATION_OUTPUT}"
    ERROR_VARIABLE JSON_MUTATION_ERROR
)
if(JSON_MUTATION_RESULT)
    message(FATAL_ERROR "JSON mutation run failed: ${JSON_MUTATION_ERROR}")
endif()
file(READ "${JSON_MUTATION_OUTPUT}" JSON_MUTATION_TEXT)
if(NOT JSON_MUTATION_TEXT MATCHES "Marila 2.*4")
    message(FATAL_ERROR "JSON mutation output was unexpected: ${JSON_MUTATION_TEXT}")
endif()

set(JSON_ESCAPE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-json-escape.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/json_escape_demo.marila"
    RESULT_VARIABLE JSON_ESCAPE_RESULT
    OUTPUT_FILE "${JSON_ESCAPE_OUTPUT}"
    ERROR_VARIABLE JSON_ESCAPE_ERROR
)
if(JSON_ESCAPE_RESULT)
    message(FATAL_ERROR "JSON escape run failed: ${JSON_ESCAPE_ERROR}")
endif()
file(READ "${JSON_ESCAPE_OUTPUT}" JSON_ESCAPE_TEXT)
if(NOT JSON_ESCAPE_TEXT MATCHES "Marila \"math\".*line one.*line two.*C:\\\\data")
    message(FATAL_ERROR "JSON escape output was unexpected: ${JSON_ESCAPE_TEXT}")
endif()

set(HIGHER_ORDER_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-higher-order.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/higher_order_demo.marila"
    RESULT_VARIABLE HIGHER_ORDER_RESULT
    OUTPUT_FILE "${HIGHER_ORDER_OUTPUT}"
    ERROR_VARIABLE HIGHER_ORDER_ERROR
)
if(HIGHER_ORDER_RESULT)
    message(FATAL_ERROR "higher-order run failed: ${HIGHER_ORDER_ERROR}")
endif()
file(READ "${HIGHER_ORDER_OUTPUT}" HIGHER_ORDER_TEXT)
if(NOT HIGHER_ORDER_TEXT MATCHES "\\[2, 4, 6, 8, 10\\].*\\[2, 4\\].*15.*true.*true.*\\[\\[1, 2\\], \\[2, 4\\]")
    message(FATAL_ERROR "higher-order output was unexpected: ${HIGHER_ORDER_TEXT}")
endif()

set(ASSERT_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-assert.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/assert_demo.marila"
    RESULT_VARIABLE ASSERT_RESULT
    OUTPUT_FILE "${ASSERT_OUTPUT}"
    ERROR_VARIABLE ASSERT_ERROR
)
if(ASSERT_RESULT OR NOT ASSERT_ERROR STREQUAL "")
    message(FATAL_ERROR "assert demo failed: ${ASSERT_ERROR}")
endif()
file(READ "${ASSERT_OUTPUT}" ASSERT_TEXT)
if(NOT ASSERT_TEXT MATCHES "assertions passed")
    message(FATAL_ERROR "assert demo output was unexpected: ${ASSERT_TEXT}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" test "${SOURCE_DIR}/examples/assert_demo.marila"
    RESULT_VARIABLE TEST_COMMAND_RESULT
    OUTPUT_VARIABLE TEST_COMMAND_OUTPUT
    ERROR_VARIABLE TEST_COMMAND_ERROR
)
if(TEST_COMMAND_RESULT OR NOT TEST_COMMAND_OUTPUT MATCHES "PASS:")
    message(FATAL_ERROR "test command failed: ${TEST_COMMAND_OUTPUT}${TEST_COMMAND_ERROR}")
endif()

set(TYPED_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-typed.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/typed_demo.marila"
    RESULT_VARIABLE TYPED_RESULT
    OUTPUT_FILE "${TYPED_OUTPUT}"
    ERROR_VARIABLE TYPED_ERROR
)
if(TYPED_RESULT)
    message(FATAL_ERROR "typed program failed: ${TYPED_ERROR}")
endif()
file(READ "${TYPED_OUTPUT}" TYPED_TEXT)
if(NOT TYPED_TEXT MATCHES "samples.*6.*true")
    message(FATAL_ERROR "typed program output was unexpected: ${TYPED_TEXT}")
endif()

set(TYPED_FUNCTION_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-typed-function.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/typed_function_demo.marila"
    RESULT_VARIABLE TYPED_FUNCTION_RESULT
    OUTPUT_FILE "${TYPED_FUNCTION_OUTPUT}"
    ERROR_VARIABLE TYPED_FUNCTION_ERROR
)
if(TYPED_FUNCTION_RESULT)
    message(FATAL_ERROR "typed function failed: ${TYPED_FUNCTION_ERROR}")
endif()
file(READ "${TYPED_FUNCTION_OUTPUT}" TYPED_FUNCTION_TEXT)
if(NOT TYPED_FUNCTION_TEXT MATCHES "10")
    message(FATAL_ERROR "typed function output was unexpected: ${TYPED_FUNCTION_TEXT}")
endif()

set(TYPED_RETURN_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-typed-return.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/typed_return_demo.marila"
    RESULT_VARIABLE TYPED_RETURN_RESULT
    OUTPUT_FILE "${TYPED_RETURN_OUTPUT}"
    ERROR_VARIABLE TYPED_RETURN_ERROR
)
if(TYPED_RETURN_RESULT)
    message(FATAL_ERROR "typed return function failed: ${TYPED_RETURN_ERROR}")
endif()
file(READ "${TYPED_RETURN_OUTPUT}" TYPED_RETURN_TEXT)
if(NOT TYPED_RETURN_TEXT MATCHES "5")
    message(FATAL_ERROR "typed return output was unexpected: ${TYPED_RETURN_TEXT}")
endif()

set(FORMAT_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-format.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" format "${SOURCE_DIR}/examples/higher_order_demo.marila"
    RESULT_VARIABLE FORMAT_RESULT
    OUTPUT_FILE "${FORMAT_OUTPUT}"
    ERROR_VARIABLE FORMAT_ERROR
)
if(FORMAT_RESULT)
    message(FATAL_ERROR "format command failed: ${FORMAT_ERROR}")
endif()
file(READ "${FORMAT_OUTPUT}" FORMAT_TEXT)
if(NOT FORMAT_TEXT MATCHES "  let values")
    message(FATAL_ERROR "format command did not normalize indentation")
endif()

set(FORMATTED_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-formatted.marila")
file(WRITE "${FORMATTED_SOURCE}" "file type program:\n[\n  let values = [1, 2, 3]\n  print sum(values)\n]\n")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" format "${FORMATTED_SOURCE}" --check
    RESULT_VARIABLE FORMATTED_CHECK_RESULT
    OUTPUT_VARIABLE FORMATTED_CHECK_OUTPUT
    ERROR_VARIABLE FORMATTED_CHECK_ERROR
)
if(FORMATTED_CHECK_RESULT)
    message(FATAL_ERROR "format --check rejected formatted source: ${FORMATTED_CHECK_ERROR}")
endif()

set(UNFORMATTED_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-unformatted.marila")
file(WRITE "${UNFORMATTED_SOURCE}" "file type program:\n[\nlet values = [1, 2, 3]\nprint sum(values)\n]\n")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" format "${UNFORMATTED_SOURCE}" --check
    RESULT_VARIABLE UNFORMATTED_CHECK_RESULT
    OUTPUT_VARIABLE UNFORMATTED_CHECK_OUTPUT
    ERROR_VARIABLE UNFORMATTED_CHECK_ERROR
)
if(NOT UNFORMATTED_CHECK_RESULT)
    message(FATAL_ERROR "format --check unexpectedly accepted an unformatted source")
endif()

set(SINGLE_QUOTE_LINT_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-single-quote-lint.marila")
file(WRITE "${SINGLE_QUOTE_LINT_SOURCE}" "file type program:\n[\nlet text = 'closing ] and } delimiters'\nprint text\n]\n")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" lint "${SINGLE_QUOTE_LINT_SOURCE}"
    RESULT_VARIABLE SINGLE_QUOTE_LINT_RESULT
    ERROR_VARIABLE SINGLE_QUOTE_LINT_ERROR
)
if(SINGLE_QUOTE_LINT_RESULT)
    message(FATAL_ERROR "lint mishandled delimiters in a single-quoted string: ${SINGLE_QUOTE_LINT_ERROR}")
endif()

set(WRITABLE_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-format-write.marila")
file(WRITE "${WRITABLE_SOURCE}" "file type program:\n[\nlet values = [1, 2, 3]\nprint sum(values)\n]\n")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" format "${WRITABLE_SOURCE}" --write
    RESULT_VARIABLE FORMAT_WRITE_RESULT
    OUTPUT_VARIABLE FORMAT_WRITE_OUTPUT
    ERROR_VARIABLE FORMAT_WRITE_ERROR
)
if(FORMAT_WRITE_RESULT)
    message(FATAL_ERROR "format --write failed: ${FORMAT_WRITE_ERROR}")
endif()
file(READ "${WRITABLE_SOURCE}" FORMAT_WRITE_TEXT)
if(NOT FORMAT_WRITE_TEXT MATCHES "  let values")
    message(FATAL_ERROR "format --write did not update the source")
endif()

set(FORMAT_LITERAL_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-format-literals.marila")
file(WRITE "${FORMAT_LITERAL_SOURCE}" "file type program:\n[\nlet record = {\"text\": \"}\"}\nif true {\nprint 'single ['\n}\n]\n")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" format "${FORMAT_LITERAL_SOURCE}"
    RESULT_VARIABLE FORMAT_LITERAL_RESULT
    OUTPUT_VARIABLE FORMAT_LITERAL_TEXT
    ERROR_VARIABLE FORMAT_LITERAL_ERROR
)
if(FORMAT_LITERAL_RESULT OR NOT FORMAT_LITERAL_TEXT MATCHES "  if true" OR NOT FORMAT_LITERAL_TEXT MATCHES "    print 'single")
    message(FATAL_ERROR "formatter mishandled quoted delimiters: ${FORMAT_LITERAL_TEXT}${FORMAT_LITERAL_ERROR}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" format "${SOURCE_DIR}/tests/smoke-format-block-comment.marila"
    RESULT_VARIABLE FORMAT_BLOCK_COMMENT_RESULT
    OUTPUT_VARIABLE FORMAT_BLOCK_COMMENT_TEXT
    ERROR_VARIABLE FORMAT_BLOCK_COMMENT_ERROR
)
if(FORMAT_BLOCK_COMMENT_RESULT OR NOT FORMAT_BLOCK_COMMENT_TEXT MATCHES "  if true" OR NOT FORMAT_BLOCK_COMMENT_TEXT MATCHES "    print 1")
    message(FATAL_ERROR "formatter mishandled block-comment delimiters: ${FORMAT_BLOCK_COMMENT_TEXT}${FORMAT_BLOCK_COMMENT_ERROR}")
endif()

set(FORMAT_SCENE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-format-scene.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" format "${SOURCE_DIR}/examples/scene_geometry_demo.marila"
    RESULT_VARIABLE FORMAT_SCENE_RESULT
    OUTPUT_FILE "${FORMAT_SCENE_OUTPUT}"
    ERROR_VARIABLE FORMAT_SCENE_ERROR
)
if(FORMAT_SCENE_RESULT)
    message(FATAL_ERROR "renderer format command failed: ${FORMAT_SCENE_ERROR}")
endif()
file(READ "${FORMAT_SCENE_OUTPUT}" FORMAT_SCENE_TEXT)
if(NOT FORMAT_SCENE_TEXT MATCHES "  point A")
    message(FATAL_ERROR "renderer formatter did not normalize statement indentation")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" format "${SOURCE_DIR}/examples/invalid_program.marila"
    RESULT_VARIABLE INVALID_FORMAT_RESULT
    OUTPUT_VARIABLE INVALID_FORMAT_OUTPUT
    ERROR_VARIABLE INVALID_FORMAT_ERROR
)
if(NOT INVALID_FORMAT_RESULT)
    message(FATAL_ERROR "format unexpectedly accepted invalid program")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/invalid_json_demo.marila"
    RESULT_VARIABLE INVALID_JSON_RESULT
    OUTPUT_VARIABLE INVALID_JSON_OUTPUT
    ERROR_VARIABLE INVALID_JSON_ERROR
)
if(NOT INVALID_JSON_RESULT OR NOT INVALID_JSON_ERROR MATCHES "trailing content")
    message(FATAL_ERROR "invalid JSON was unexpectedly accepted: ${INVALID_JSON_OUTPUT}${INVALID_JSON_ERROR}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/invalid_type_demo.marila"
    RESULT_VARIABLE INVALID_TYPE_RESULT
    OUTPUT_VARIABLE INVALID_TYPE_OUTPUT
    ERROR_VARIABLE INVALID_TYPE_ERROR
)
if(NOT INVALID_TYPE_RESULT OR NOT INVALID_TYPE_ERROR MATCHES "not of type number")
    message(FATAL_ERROR "invalid type was unexpectedly accepted: ${INVALID_TYPE_OUTPUT}${INVALID_TYPE_ERROR}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/circular_import_demo.marila"
    RESULT_VARIABLE CIRCULAR_IMPORT_RESULT
    OUTPUT_VARIABLE CIRCULAR_IMPORT_OUTPUT
    ERROR_VARIABLE CIRCULAR_IMPORT_ERROR
)
if(NOT CIRCULAR_IMPORT_RESULT OR NOT CIRCULAR_IMPORT_ERROR MATCHES "circular import")
    message(FATAL_ERROR "circular import was not diagnosed: ${CIRCULAR_IMPORT_OUTPUT}${CIRCULAR_IMPORT_ERROR}")
endif()

set(OBJECT_LITERAL_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-object-literal.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/object_literal_demo.marila"
    RESULT_VARIABLE OBJECT_LITERAL_RESULT
    OUTPUT_FILE "${OBJECT_LITERAL_OUTPUT}"
    ERROR_VARIABLE OBJECT_LITERAL_ERROR
)
if(OBJECT_LITERAL_RESULT)
    message(FATAL_ERROR "object literal run failed: ${OBJECT_LITERAL_ERROR}")
endif()
file(READ "${OBJECT_LITERAL_OUTPUT}" OBJECT_LITERAL_TEXT)
if(NOT OBJECT_LITERAL_TEXT MATCHES "Marila.*graphics.*true")
    message(FATAL_ERROR "object literal output was unexpected: ${OBJECT_LITERAL_TEXT}")
endif()

set(REPL_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-repl.txt")
set(REPL_INPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-repl-input.txt")
file(WRITE "${REPL_INPUT}" "let good = 4\nlet bad = ]\nprint good\nquit\n")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" repl
    INPUT_FILE "${REPL_INPUT}"
    OUTPUT_FILE "${REPL_OUTPUT}"
    ERROR_VARIABLE REPL_ERROR
    RESULT_VARIABLE REPL_RESULT
)
if(REPL_RESULT)
    message(FATAL_ERROR "repl did not recover from an input error: ${REPL_ERROR}")
endif()
file(READ "${REPL_OUTPUT}" REPL_TEXT)
if(NOT REPL_TEXT MATCHES "4")
    message(FATAL_ERROR "repl did not continue after an input error: ${REPL_TEXT}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" lint "${SOURCE_DIR}/examples/invalid_program.marila"
    RESULT_VARIABLE INVALID_LINT_RESULT
)
if(INVALID_LINT_RESULT EQUAL 0)
    message(FATAL_ERROR "lint unexpectedly accepted invalid program")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" check "${SOURCE_DIR}/examples/invalid_renderer.marila"
    RESULT_VARIABLE INVALID_RENDERER_CHECK_RESULT
    ERROR_VARIABLE INVALID_RENDERER_CHECK_ERROR
)
if(NOT INVALID_RENDERER_CHECK_RESULT OR NOT INVALID_RENDERER_CHECK_ERROR MATCHES "unbalanced source delimiters")
    message(FATAL_ERROR "typed-source check did not diagnose malformed renderer input")
endif()

set(INVALID_VISUAL_DIAGNOSE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-invalid-visual-diagnose.json")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" diagnose "${SOURCE_DIR}/examples/invalid_visual_statement.marila"
    RESULT_VARIABLE INVALID_VISUAL_DIAGNOSE_RESULT
    OUTPUT_FILE "${INVALID_VISUAL_DIAGNOSE_OUTPUT}"
    ERROR_VARIABLE INVALID_VISUAL_DIAGNOSE_ERROR
)
if(NOT INVALID_VISUAL_DIAGNOSE_RESULT)
    message(FATAL_ERROR "diagnose unexpectedly accepted invalid visual statements")
endif()
file(READ "${INVALID_VISUAL_DIAGNOSE_OUTPUT}" INVALID_VISUAL_DIAGNOSE_TEXT)
if(NOT INVALID_VISUAL_DIAGNOSE_TEXT MATCHES "\"line\":5" OR
   NOT INVALID_VISUAL_DIAGNOSE_TEXT MATCHES "\"column\":1" OR
   NOT INVALID_VISUAL_DIAGNOSE_TEXT MATCHES "unknown visual statement" OR
   NOT INVALID_VISUAL_DIAGNOSE_TEXT MATCHES "unknown point")
    message(FATAL_ERROR "visual diagnostics did not include source locations and reference errors: ${INVALID_VISUAL_DIAGNOSE_TEXT}")
endif()

set(INTEROP_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-interop.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/interop_demo.marila"
    RESULT_VARIABLE INTEROP_RESULT
    OUTPUT_FILE "${INTEROP_OUTPUT}"
    ERROR_VARIABLE INTEROP_ERROR
)
if(INTEROP_RESULT)
    message(FATAL_ERROR "interop run failed: ${INTEROP_ERROR}")
endif()
file(READ "${INTEROP_OUTPUT}" INTEROP_TEXT)
if(NOT INTEROP_TEXT MATCHES "\\[2, 3\\].*\\[2, 3\\]")
    message(FATAL_ERROR "CSV/JSON round-trip shape was not preserved: ${INTEROP_TEXT}")
endif()

set(TEXT_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-text.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/text_demo.marila"
    RESULT_VARIABLE TEXT_RESULT
    OUTPUT_FILE "${TEXT_OUTPUT}"
    ERROR_VARIABLE TEXT_ERROR
)
if(TEXT_RESULT)
    message(FATAL_ERROR "text operation run failed: ${TEXT_ERROR}")
endif()
file(READ "${TEXT_OUTPUT}" TEXT_TEXT)
if(NOT TEXT_TEXT MATCHES "Marila language.*MARILA LANGUAGE.*math \| graphics \| documents.*true.*15.*43")
    message(FATAL_ERROR "text operations output was unexpected: ${TEXT_TEXT}")
endif()

set(GRID_CALCULUS_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-grid-calculus.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/grid_calculus_demo.marila"
    RESULT_VARIABLE GRID_CALCULUS_RESULT
    OUTPUT_FILE "${GRID_CALCULUS_OUTPUT}"
    ERROR_VARIABLE GRID_CALCULUS_ERROR
)
if(GRID_CALCULUS_RESULT)
    message(FATAL_ERROR "grid calculus run failed: ${GRID_CALCULUS_ERROR}")
endif()
file(READ "${GRID_CALCULUS_OUTPUT}" GRID_CALCULUS_TEXT)
if(NOT GRID_CALCULUS_TEXT MATCHES "\\[2, 3\\].*\\[1, 2, 4, 5\\].*4.5")
    message(FATAL_ERROR "grid calculus output was unexpected: ${GRID_CALCULUS_TEXT}")
endif()

set(MATRIX_SLICE_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-matrix-slice.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/matrix_slice_demo.marila"
    RESULT_VARIABLE MATRIX_SLICE_RESULT
    OUTPUT_FILE "${MATRIX_SLICE_OUTPUT}"
    ERROR_VARIABLE MATRIX_SLICE_ERROR
)
if(MATRIX_SLICE_RESULT)
    message(FATAL_ERROR "matrix slice run failed: ${MATRIX_SLICE_ERROR}")
endif()
file(READ "${MATRIX_SLICE_OUTPUT}" MATRIX_SLICE_TEXT)
if(NOT MATRIX_SLICE_TEXT MATCHES "\\[\\[20, 30\\], \\[40, 50\\]\\].*\\[4, 40\\].*\\[30, 50\\]")
    message(FATAL_ERROR "matrix slice output was unexpected: ${MATRIX_SLICE_TEXT}")
endif()

set(LOOP_CONTROL_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-loop-control.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/loop_control_demo.marila"
    RESULT_VARIABLE LOOP_CONTROL_RESULT
    OUTPUT_FILE "${LOOP_CONTROL_OUTPUT}"
    ERROR_VARIABLE LOOP_CONTROL_ERROR
)
if(LOOP_CONTROL_RESULT)
    message(FATAL_ERROR "loop control run failed: ${LOOP_CONTROL_ERROR}")
endif()
file(READ "${LOOP_CONTROL_OUTPUT}" LOOP_CONTROL_TEXT)
if(NOT LOOP_CONTROL_TEXT MATCHES "4")
    message(FATAL_ERROR "loop control output was unexpected: ${LOOP_CONTROL_TEXT}")
endif()

set(ERROR_HANDLING_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-error-handling.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/error_handling_demo.marila"
    RESULT_VARIABLE ERROR_HANDLING_RESULT
    OUTPUT_FILE "${ERROR_HANDLING_OUTPUT}"
    ERROR_VARIABLE ERROR_HANDLING_ERROR
)
if(ERROR_HANDLING_RESULT)
    message(FATAL_ERROR "error handling run failed: ${ERROR_HANDLING_ERROR}")
endif()
file(READ "${ERROR_HANDLING_OUTPUT}" ERROR_HANDLING_TEXT)
if(NOT ERROR_HANDLING_TEXT MATCHES "recoverable failure.*conversion failed: .*number expects a numeric value")
    message(FATAL_ERROR "error handling output was unexpected: ${ERROR_HANDLING_TEXT}")
endif()

set(FINALLY_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-finally.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/finally_demo.marila"
    RESULT_VARIABLE FINALLY_RESULT
    OUTPUT_FILE "${FINALLY_OUTPUT}"
    ERROR_VARIABLE FINALLY_ERROR
)
if(FINALLY_RESULT)
    message(FATAL_ERROR "finally demo failed: ${FINALLY_ERROR}")
endif()
file(READ "${FINALLY_OUTPUT}" FINALLY_TEXT)
if(NOT FINALLY_TEXT MATCHES "2")
    message(FATAL_ERROR "finally block did not execute: ${FINALLY_TEXT}")
endif()

set(ITERATION_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-iteration.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/iteration_demo.marila"
    RESULT_VARIABLE ITERATION_RESULT
    OUTPUT_FILE "${ITERATION_OUTPUT}"
    ERROR_VARIABLE ITERATION_ERROR
)
if(ITERATION_RESULT)
    message(FATAL_ERROR "iteration demo failed: ${ITERATION_ERROR}")
endif()
file(READ "${ITERATION_OUTPUT}" ITERATION_TEXT)
if(NOT ITERATION_TEXT MATCHES "31" OR NOT ITERATION_TEXT MATCHES "ab")
    message(FATAL_ERROR "two-variable iteration output was unexpected: ${ITERATION_TEXT}")
endif()

set(STRING_ITERATION_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-string-iteration.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/string_iteration_demo.marila"
    RESULT_VARIABLE STRING_ITERATION_RESULT
    OUTPUT_FILE "${STRING_ITERATION_OUTPUT}"
    ERROR_VARIABLE STRING_ITERATION_ERROR
)
if(STRING_ITERATION_RESULT)
    message(FATAL_ERROR "string iteration failed: ${STRING_ITERATION_ERROR}")
endif()
file(READ "${STRING_ITERATION_OUTPUT}" STRING_ITERATION_TEXT)
if(NOT STRING_ITERATION_TEXT MATCHES "Marila")
    message(FATAL_ERROR "string iteration output was unexpected: ${STRING_ITERATION_TEXT}")
endif()

set(COMPOUND_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-compound.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/compound_assignment_demo.marila"
    RESULT_VARIABLE COMPOUND_RESULT
    OUTPUT_FILE "${COMPOUND_OUTPUT}"
    ERROR_VARIABLE COMPOUND_ERROR
)
if(COMPOUND_RESULT)
    message(FATAL_ERROR "compound assignment demo failed: ${COMPOUND_ERROR}")
endif()
file(READ "${COMPOUND_OUTPUT}" COMPOUND_TEXT)
if(NOT COMPOUND_TEXT MATCHES "1")
    message(FATAL_ERROR "compound assignment output was unexpected: ${COMPOUND_TEXT}")
endif()

set(INDEXED_COMPOUND_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-indexed-compound.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/indexed_compound_demo.marila"
    RESULT_VARIABLE INDEXED_COMPOUND_RESULT
    OUTPUT_FILE "${INDEXED_COMPOUND_OUTPUT}"
    ERROR_VARIABLE INDEXED_COMPOUND_ERROR
)
if(INDEXED_COMPOUND_RESULT)
    message(FATAL_ERROR "indexed compound assignment failed: ${INDEXED_COMPOUND_ERROR}")
endif()
file(READ "${INDEXED_COMPOUND_OUTPUT}" INDEXED_COMPOUND_TEXT)
if(NOT INDEXED_COMPOUND_TEXT MATCHES "\[5, 6\]")
    message(FATAL_ERROR "indexed compound assignment output was unexpected: ${INDEXED_COMPOUND_TEXT}")
endif()

set(UNICODE_ITERATION_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-unicode-iteration.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/unicode_iteration_demo.marila"
    RESULT_VARIABLE UNICODE_ITERATION_RESULT
    OUTPUT_FILE "${UNICODE_ITERATION_OUTPUT}"
    ERROR_VARIABLE UNICODE_ITERATION_ERROR
)
if(UNICODE_ITERATION_RESULT)
    message(FATAL_ERROR "Unicode string iteration failed: ${UNICODE_ITERATION_ERROR}")
endif()
file(READ "${UNICODE_ITERATION_OUTPUT}" UNICODE_ITERATION_TEXT)
if(NOT UNICODE_ITERATION_TEXT MATCHES "Aλ中")
    message(FATAL_ERROR "Unicode string iteration output was unexpected: ${UNICODE_ITERATION_TEXT}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/invalid_dimensions.marila"
    RESULT_VARIABLE INVALID_DIMENSIONS_RESULT
    ERROR_VARIABLE INVALID_DIMENSIONS_ERROR
)
if(NOT INVALID_DIMENSIONS_RESULT OR NOT INVALID_DIMENSIONS_ERROR MATCHES "non-negative integer")
    message(FATAL_ERROR "invalid matrix dimensions were accepted: ${INVALID_DIMENSIONS_ERROR}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/invalid_interop_type.marila"
    RESULT_VARIABLE INVALID_INTEROP_RESULT
    ERROR_VARIABLE INVALID_INTEROP_ERROR
)
if(NOT INVALID_INTEROP_RESULT OR NOT INVALID_INTEROP_ERROR MATCHES "read_json expects a file path")
    message(FATAL_ERROR "invalid interop argument was not diagnosed clearly: ${INVALID_INTEROP_ERROR}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/invalid_text_type.marila"
    RESULT_VARIABLE INVALID_TEXT_TYPE_RESULT
    ERROR_VARIABLE INVALID_TEXT_TYPE_ERROR
)
if(NOT INVALID_TEXT_TYPE_RESULT OR NOT INVALID_TEXT_TYPE_ERROR MATCHES "split expects a string and a delimiter")
    message(FATAL_ERROR "invalid text argument types were not diagnosed: ${INVALID_TEXT_TYPE_ERROR}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/invalid_object_helper_type.marila"
    RESULT_VARIABLE INVALID_OBJECT_HELPER_RESULT
    ERROR_VARIABLE INVALID_OBJECT_HELPER_ERROR
)
if(NOT INVALID_OBJECT_HELPER_RESULT OR NOT INVALID_OBJECT_HELPER_ERROR MATCHES "keys expects one object")
    message(FATAL_ERROR "invalid object helper types were not diagnosed: ${INVALID_OBJECT_HELPER_ERROR}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/invalid_shape.marila"
    RESULT_VARIABLE INVALID_SHAPE_RESULT
    ERROR_VARIABLE INVALID_SHAPE_ERROR
)
if(NOT INVALID_SHAPE_RESULT OR NOT INVALID_SHAPE_ERROR MATCHES "rectangular matrix")
    message(FATAL_ERROR "ragged shape was not rejected: ${INVALID_SHAPE_ERROR}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/invalid_empty_matrix.marila"
    RESULT_VARIABLE INVALID_EMPTY_MATRIX_RESULT
    ERROR_VARIABLE INVALID_EMPTY_MATRIX_ERROR
)
if(NOT INVALID_EMPTY_MATRIX_RESULT OR NOT INVALID_EMPTY_MATRIX_ERROR MATCHES "rectangular and non-empty")
    message(FATAL_ERROR "empty matrix row was not rejected: ${INVALID_EMPTY_MATRIX_ERROR}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/invalid_control_flow.marila"
    RESULT_VARIABLE INVALID_CONTROL_FLOW_RESULT
    ERROR_VARIABLE INVALID_CONTROL_FLOW_ERROR
)
if(NOT INVALID_CONTROL_FLOW_RESULT OR NOT INVALID_CONTROL_FLOW_ERROR MATCHES "return outside a function")
    message(FATAL_ERROR "invalid top-level control flow was not diagnosed: ${INVALID_CONTROL_FLOW_ERROR}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" check "${SOURCE_DIR}/examples/invalid_block_comment.marila"
    RESULT_VARIABLE INVALID_BLOCK_COMMENT_RESULT
    ERROR_VARIABLE INVALID_BLOCK_COMMENT_ERROR
)
if(NOT INVALID_BLOCK_COMMENT_RESULT OR NOT INVALID_BLOCK_COMMENT_ERROR MATCHES "unterminated block comment")
    message(FATAL_ERROR "unterminated block comment was not diagnosed: ${INVALID_BLOCK_COMMENT_ERROR}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" check "${SOURCE_DIR}/examples/invalid_string.marila"
    RESULT_VARIABLE INVALID_STRING_RESULT
    ERROR_VARIABLE INVALID_STRING_ERROR
)
if(NOT INVALID_STRING_RESULT OR NOT INVALID_STRING_ERROR MATCHES "line 3, column 12: unterminated string")
    message(FATAL_ERROR "unterminated string location was not diagnosed: ${INVALID_STRING_ERROR}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/invalid_numeric_arguments.marila"
    RESULT_VARIABLE INVALID_NUMERIC_ARGUMENT_RESULT
    ERROR_VARIABLE INVALID_NUMERIC_ARGUMENT_ERROR
)
if(NOT INVALID_NUMERIC_ARGUMENT_RESULT OR NOT INVALID_NUMERIC_ARGUMENT_ERROR MATCHES "random rows must be a non-negative integer")
    message(FATAL_ERROR "fractional numerical dimensions were not diagnosed: ${INVALID_NUMERIC_ARGUMENT_ERROR}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/invalid_index.marila"
    RESULT_VARIABLE INVALID_INDEX_RESULT
    ERROR_VARIABLE INVALID_INDEX_ERROR
)
if(NOT INVALID_INDEX_RESULT OR NOT INVALID_INDEX_ERROR MATCHES "array index must be a non-negative integer")
    message(FATAL_ERROR "fractional array indices were not diagnosed: ${INVALID_INDEX_ERROR}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/invalid_diag_offset.marila"
    RESULT_VARIABLE INVALID_DIAG_OFFSET_RESULT
    ERROR_VARIABLE INVALID_DIAG_OFFSET_ERROR
)
if(NOT INVALID_DIAG_OFFSET_RESULT OR NOT INVALID_DIAG_OFFSET_ERROR MATCHES "diag offset must be an integer")
    message(FATAL_ERROR "fractional diagonal offsets were not diagnosed: ${INVALID_DIAG_OFFSET_ERROR}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/invalid_linspace.marila"
    RESULT_VARIABLE INVALID_LINSPACE_RESULT
    ERROR_VARIABLE INVALID_LINSPACE_ERROR
)
if(NOT INVALID_LINSPACE_RESULT OR NOT INVALID_LINSPACE_ERROR MATCHES "linspace count must be a non-negative integer")
    message(FATAL_ERROR "invalid linspace count was not diagnosed: ${INVALID_LINSPACE_ERROR}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/invalid_range.marila"
    RESULT_VARIABLE INVALID_RANGE_RESULT
    ERROR_VARIABLE INVALID_RANGE_ERROR
)
if(NOT INVALID_RANGE_RESULT OR NOT INVALID_RANGE_ERROR MATCHES "numeric literal out of range")
    message(FATAL_ERROR "non-finite range values were not diagnosed: ${INVALID_RANGE_ERROR}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/invalid_interp1.marila"
    RESULT_VARIABLE INVALID_INTERP1_RESULT
    ERROR_VARIABLE INVALID_INTERP1_ERROR
)
if(NOT INVALID_INTERP1_RESULT OR NOT INVALID_INTERP1_ERROR MATCHES "interp1 x samples must be strictly increasing")
    message(FATAL_ERROR "unsorted interp1 samples were not diagnosed: ${INVALID_INTERP1_ERROR}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" check "${SOURCE_DIR}/examples/invalid_animation_timeline.marila"
    RESULT_VARIABLE INVALID_TIMELINE_RESULT
    ERROR_VARIABLE INVALID_TIMELINE_ERROR
)
if(NOT INVALID_TIMELINE_RESULT OR NOT INVALID_TIMELINE_ERROR MATCHES "animation duration must be positive")
    message(FATAL_ERROR "invalid animation duration was not rejected: ${INVALID_TIMELINE_ERROR}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/invalid_json_number.marila"
    RESULT_VARIABLE INVALID_JSON_NUMBER_RESULT
    ERROR_VARIABLE INVALID_JSON_NUMBER_ERROR
)
if(NOT INVALID_JSON_NUMBER_RESULT OR NOT INVALID_JSON_NUMBER_ERROR MATCHES "non-finite number")
    message(FATAL_ERROR "non-finite JSON output was not rejected: ${INVALID_JSON_NUMBER_ERROR}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/invalid_json_comma.marila"
    RESULT_VARIABLE INVALID_JSON_COMMA_RESULT
    ERROR_VARIABLE INVALID_JSON_COMMA_ERROR
)
if(NOT INVALID_JSON_COMMA_RESULT OR NOT INVALID_JSON_COMMA_ERROR MATCHES "trailing comma")
    message(FATAL_ERROR "JSON trailing comma was not rejected: ${INVALID_JSON_COMMA_ERROR}")
endif()

execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/invalid_json_number_syntax.marila"
    RESULT_VARIABLE INVALID_JSON_SYNTAX_RESULT
    ERROR_VARIABLE INVALID_JSON_SYNTAX_ERROR
)
if(NOT INVALID_JSON_SYNTAX_RESULT OR NOT INVALID_JSON_SYNTAX_ERROR MATCHES "invalid JSON number")
    message(FATAL_ERROR "invalid JSON number syntax was not rejected: ${INVALID_JSON_SYNTAX_ERROR}")
endif()

set(CONDITIONAL_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-conditional.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/conditional_demo.marila"
    RESULT_VARIABLE CONDITIONAL_RESULT
    OUTPUT_FILE "${CONDITIONAL_OUTPUT}"
    ERROR_VARIABLE CONDITIONAL_ERROR
)
if(CONDITIONAL_RESULT)
    message(FATAL_ERROR "conditional run failed: ${CONDITIONAL_ERROR}")
endif()
file(READ "${CONDITIONAL_OUTPUT}" CONDITIONAL_TEXT)
if(NOT CONDITIONAL_TEXT MATCHES "two")
    message(FATAL_ERROR "else-if output was unexpected: ${CONDITIONAL_TEXT}")
endif()

set(SHORT_CIRCUIT_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-short-circuit.txt")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" run "${SOURCE_DIR}/examples/short_circuit_demo.marila"
    RESULT_VARIABLE SHORT_CIRCUIT_RESULT
    OUTPUT_FILE "${SHORT_CIRCUIT_OUTPUT}"
    ERROR_VARIABLE SHORT_CIRCUIT_ERROR
)
if(SHORT_CIRCUIT_RESULT)
    message(FATAL_ERROR "short-circuit run failed: ${SHORT_CIRCUIT_ERROR}")
endif()
file(READ "${SHORT_CIRCUIT_OUTPUT}" SHORT_CIRCUIT_TEXT)
if(NOT SHORT_CIRCUIT_TEXT MATCHES "false.*true")
    message(FATAL_ERROR "short-circuit output was unexpected: ${SHORT_CIRCUIT_TEXT}")
endif()
set(INVALID_PLOT_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/marila-smoke-invalid-plot.json")
execute_process(
    COMMAND "${MARILA_EXECUTABLE}" diagnose "${SOURCE_DIR}/examples/invalid_plot_function.marila"
    RESULT_VARIABLE INVALID_PLOT_RESULT
    OUTPUT_FILE "${INVALID_PLOT_OUTPUT}"
    ERROR_VARIABLE INVALID_PLOT_ERROR
)
if(NOT INVALID_PLOT_RESULT)
    message(FATAL_ERROR "unsupported plot functions were accepted")
endif()
file(READ "${INVALID_PLOT_OUTPUT}" INVALID_PLOT_TEXT)
if(NOT INVALID_PLOT_TEXT MATCHES "unsupported plot function")
    message(FATAL_ERROR "unsupported plot function was not diagnosed: ${INVALID_PLOT_TEXT}")
endif()
