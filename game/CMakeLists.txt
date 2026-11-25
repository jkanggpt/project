# 화면과 분리된 C 실행 계층은 그래픽 라이브러리 없이도 테스트합니다.
add_library(castle_frontend STATIC src/path.c src/session.c src/combat.c src/storage.c src/sprite_layout.c src/effects.c src/runtime.c)
target_include_directories(castle_frontend PUBLIC src PRIVATE ${PROJECT_SOURCE_DIR}/core/src)
target_link_libraries(castle_frontend PUBLIC castle_core_static PRIVATE castle_warnings)
if(NOT WIN32)
  target_link_libraries(castle_frontend PUBLIC m)
else()
  target_link_libraries(castle_frontend PUBLIC shell32)
endif()

if(CASTLE_BUILD_GAME)
  find_package(raylib 5.5 EXACT QUIET)
  if(NOT raylib_FOUND)
    include(FetchContent)
    if(POLICY CMP0135)
      cmake_policy(SET CMP0135 NEW)
    endif()
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(CUSTOMIZE_BUILD ON CACHE BOOL "" FORCE)
    set(SUPPORT_FILEFORMAT_MOD OFF CACHE BOOL "" FORCE)
    set(SUPPORT_FILEFORMAT_XM OFF CACHE BOOL "" FORCE)
    set(SUPPORT_MODULE_RMODELS OFF CACHE BOOL "" FORCE)
    set(SUPPORT_CLIPBOARD_IMAGE OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
    set(OPENGL_VERSION "2.1" CACHE STRING "" FORCE)
    set(CASTLE_RAYLIB_URL "https://codeload.github.com/raysan5/raylib/tar.gz/refs/tags/5.5")
    if(EXISTS "${PROJECT_SOURCE_DIR}/build/deps/raylib-5.5.tar.gz")
      set(CASTLE_RAYLIB_URL "${PROJECT_SOURCE_DIR}/build/deps/raylib-5.5.tar.gz")
    endif()
    FetchContent_Declare(raylib URL "${CASTLE_RAYLIB_URL}"
      URL_HASH SHA256=aea98ecf5bc5c5e0b789a76de0083a21a70457050ea4cc2aec7566935f5e258e)
    FetchContent_MakeAvailable(raylib)
    # 5.5의 설정 파서가 조건부 매크로 양쪽(7/9)을 모두 등록합니다.
    # 켜진 GPU 스키닝 분기의 9를 남겨 외부 코드의 중복 정의를 피합니다.
    get_target_property(CASTLE_RAYLIB_DEFS raylib COMPILE_DEFINITIONS)
    if("MAX_MESH_VERTEX_BUFFERS=9" IN_LIST CASTLE_RAYLIB_DEFS)
      list(FILTER CASTLE_RAYLIB_DEFS EXCLUDE REGEX "^MAX_MESH_VERTEX_BUFFERS=7$")
      set_target_properties(raylib PROPERTIES COMPILE_DEFINITIONS "${CASTLE_RAYLIB_DEFS}")
    endif()
    if(MSVC)
      target_compile_options(raylib PRIVATE /utf-8)
      if(TARGET glfw)
        target_compile_options(glfw PRIVATE /utf-8)
      endif()
    endif()
  endif()
  add_executable(castle_game src/main.c src/ui.c src/panels.c src/render.c src/assets.c src/v122_ui.c src/v123_ui.c src/v124_ui.c)
  set(CASTLE_GAME_OUTPUT_DIR "" CACHE PATH "실행 중인 게임과 분리할 선택적 검증 출력 폴더")
  if(CASTLE_GAME_OUTPUT_DIR)
    set_target_properties(castle_game PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CASTLE_GAME_OUTPUT_DIR}")
  endif()
  target_include_directories(castle_game PRIVATE ${PROJECT_SOURCE_DIR}/core/src ${PROJECT_SOURCE_DIR}/cli)
  target_link_libraries(castle_game PRIVATE castle_frontend castle_cli_support raylib castle_warnings)
  if(MINGW)
    target_link_options(castle_game PRIVATE -static)
  endif()
  # C를 다시 컴파일하지 않아도 새 PNG와 정의 파일을 실행 폴더로 동기화합니다.
  set(CASTLE_COPY_DIRECTORY copy_directory)
  if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.26")
    set(CASTLE_COPY_DIRECTORY copy_directory_if_different)
  endif()
  add_custom_target(castle_game_assets ALL
    COMMAND ${CMAKE_COMMAND} -E ${CASTLE_COPY_DIRECTORY} "${CMAKE_CURRENT_SOURCE_DIR}/data" "$<TARGET_FILE_DIR:castle_game>/data"
    COMMAND ${CMAKE_COMMAND} -E ${CASTLE_COPY_DIRECTORY} "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:castle_game>/assets")
  add_dependencies(castle_game castle_game_assets)
  install(TARGETS castle_game RUNTIME DESTINATION .)
  install(DIRECTORY data assets DESTINATION . PATTERN "*.import" EXCLUDE PATTERN ".gdignore" EXCLUDE)
  install(FILES ${PROJECT_SOURCE_DIR}/docs/V1_2_4_SPEC.md ${PROJECT_SOURCE_DIR}/docs/V1_2_4_IMPLEMENTATION.md
    ${PROJECT_SOURCE_DIR}/docs/V1_2_4_QA.md ${PROJECT_SOURCE_DIR}/docs/v124_art_source.csv
    ${PROJECT_SOURCE_DIR}/docs/v124_barter_source.csv ${PROJECT_SOURCE_DIR}/docs/v124_animal_data.json
    ${PROJECT_SOURCE_DIR}/docs/v124_art_applied.csv ${PROJECT_SOURCE_DIR}/docs/v124_species_samples.csv
    ${PROJECT_SOURCE_DIR}/docs/v124_forecast_samples.csv ${PROJECT_SOURCE_DIR}/docs/v124_merchant_samples.csv
    ${PROJECT_SOURCE_DIR}/docs/v124_recipe_frequency.csv
    ${PROJECT_SOURCE_DIR}/docs/v124_balance_results.csv ${PROJECT_SOURCE_DIR}/docs/v124_map_statistics.csv
    ${PROJECT_SOURCE_DIR}/docs/v124_performance_results.csv DESTINATION docs)
  install(FILES ${PROJECT_SOURCE_DIR}/art/V124_PROMPTS.json ${PROJECT_SOURCE_DIR}/art/V124_MANIFEST.json DESTINATION art)
  install(DIRECTORY ${PROJECT_SOURCE_DIR}/docs/screenshots/ DESTINATION docs/screenshots FILES_MATCHING PATTERN "v124*.png")
  install(FILES ${PROJECT_SOURCE_DIR}/CREDITS.md ${PROJECT_SOURCE_DIR}/README.md DESTINATION .)
  install(FILES ${PROJECT_SOURCE_DIR}/PROGRESS.md DESTINATION .)
  install(FILES ${PROJECT_SOURCE_DIR}/docs/V1_2_3_SPEC.html ${PROJECT_SOURCE_DIR}/docs/V1_2_3_SPEC.md
    ${PROJECT_SOURCE_DIR}/docs/V1_2_3_IMPLEMENTATION.md ${PROJECT_SOURCE_DIR}/docs/V1_2_3_QA.md
    ${PROJECT_SOURCE_DIR}/docs/v123_barter_recipes.csv ${PROJECT_SOURCE_DIR}/docs/v123_balance_results.csv
    ${PROJECT_SOURCE_DIR}/docs/v123_map_statistics.csv ${PROJECT_SOURCE_DIR}/docs/v123_map_summary.json
    ${PROJECT_SOURCE_DIR}/docs/v123_performance_results.csv DESTINATION docs)
  install(FILES ${PROJECT_SOURCE_DIR}/art/V123_PROMPTS.json ${PROJECT_SOURCE_DIR}/art/V123_MANIFEST.json DESTINATION art)
  install(FILES ${PROJECT_SOURCE_DIR}/docs/screenshots/v123_cards_1440.png
    ${PROJECT_SOURCE_DIR}/docs/screenshots/v123_cards_800.png ${PROJECT_SOURCE_DIR}/docs/screenshots/v123_cards_1024.png
    ${PROJECT_SOURCE_DIR}/docs/screenshots/v123_gates_1024.png ${PROJECT_SOURCE_DIR}/docs/screenshots/v123merchant_1440.png
    ${PROJECT_SOURCE_DIR}/docs/screenshots/v123boats_1440.png ${PROJECT_SOURCE_DIR}/docs/screenshots/v123home_1440.png
    ${PROJECT_SOURCE_DIR}/docs/screenshots/v123forecast_1440.png ${PROJECT_SOURCE_DIR}/docs/screenshots/v123buildings_1440.png DESTINATION docs/screenshots)
  install(FILES ${PROJECT_SOURCE_DIR}/docs/V1_2_2_PATCH2.md DESTINATION docs)
  install(FILES ${PROJECT_SOURCE_DIR}/docs/screenshots/v122_patch2_selection_1440.png
    ${PROJECT_SOURCE_DIR}/docs/screenshots/v122_patch2_demolition_1024.png
    ${PROJECT_SOURCE_DIR}/docs/screenshots/v122_patch2_workforce_1024.png
    ${PROJECT_SOURCE_DIR}/docs/screenshots/v122_patch2_trade_1024.png
    ${PROJECT_SOURCE_DIR}/docs/screenshots/v122_patch2_skills_1024.png DESTINATION docs/screenshots)
  install(FILES ${PROJECT_SOURCE_DIR}/docs/V1_2_2_PATCH1.md ${PROJECT_SOURCE_DIR}/docs/v122_patch1_checks.csv DESTINATION docs)
  install(FILES ${PROJECT_SOURCE_DIR}/art/V122_PATCH1_PROMPTS.json DESTINATION art)
  install(FILES ${PROJECT_SOURCE_DIR}/docs/screenshots/v122_patch1_game_1440.png
    ${PROJECT_SOURCE_DIR}/docs/screenshots/v122_patch1_population_1024.png
    ${PROJECT_SOURCE_DIR}/docs/screenshots/v122_patch1_raid_1024.png
    ${PROJECT_SOURCE_DIR}/docs/screenshots/v122_patch1_tower_1024.png
    ${PROJECT_SOURCE_DIR}/docs/screenshots/v122_patch1_assign_1024.png
    ${PROJECT_SOURCE_DIR}/docs/screenshots/v122_patch1_alerts_1024.png
    ${PROJECT_SOURCE_DIR}/docs/screenshots/v122_patch1_effects_1440.png DESTINATION docs/screenshots)
  install(FILES ${PROJECT_SOURCE_DIR}/art/README.md ${PROJECT_SOURCE_DIR}/art/PROMPTS.md
    ${PROJECT_SOURCE_DIR}/art/PROMPTS_OVERHEAD.md ${PROJECT_SOURCE_DIR}/art/EXPANSION_PROMPTS.md
    ${PROJECT_SOURCE_DIR}/art/EXPANSION_ICONS_PROMPTS.md
    ${PROJECT_SOURCE_DIR}/art/TOPOLOGY_ANIMATION_PROMPTS.md
    ${PROJECT_SOURCE_DIR}/art/V111_PROMPTS.md ${PROJECT_SOURCE_DIR}/art/V121_PROMPTS.md DESTINATION art)
  install(FILES ${PROJECT_SOURCE_DIR}/art/expansion/ALPHA_REPAIR.md DESTINATION art/expansion)
  install(FILES ${PROJECT_SOURCE_DIR}/art/V122_PROMPTS.md
    ${PROJECT_SOURCE_DIR}/art/V122_RESIDENT_PROMPTS.json
    ${PROJECT_SOURCE_DIR}/art/V122_UNIFORM_PROMPTS.json
    ${PROJECT_SOURCE_DIR}/art/V122_FARM_PROMPTS.json
    ${PROJECT_SOURCE_DIR}/art/V122_FARM_ADDITIONAL_PROMPT.json
    ${PROJECT_SOURCE_DIR}/art/V122_MISC_PROMPTS.json
    ${PROJECT_SOURCE_DIR}/art/V122_CORRECTION_PROMPTS_FINAL.json
    ${PROJECT_SOURCE_DIR}/art/V122_YELLOW_WALK_PROMPTS.json
    ${PROJECT_SOURCE_DIR}/art/V122_ARCHERY_RETRY_PROMPTS.json
    ${PROJECT_SOURCE_DIR}/art/V122_ALPHA_PROMPTS.json DESTINATION art)
  install(FILES ${PROJECT_SOURCE_DIR}/docs/NATIVE_PORT.md ${PROJECT_SOURCE_DIR}/docs/ARCHITECTURE.md
    ${PROJECT_SOURCE_DIR}/docs/QA.md
    ${PROJECT_SOURCE_DIR}/docs/V1_1_1_SPEC.md
    ${PROJECT_SOURCE_DIR}/docs/V1_1_REFERENCE.md
    ${PROJECT_SOURCE_DIR}/docs/V1_1_1_IMPLEMENTATION.md
    ${PROJECT_SOURCE_DIR}/docs/V1_1_1_QA.md
    ${PROJECT_SOURCE_DIR}/docs/v111_balance_results.csv DESTINATION docs)
  install(FILES ${PROJECT_SOURCE_DIR}/docs/V1_2_1_SPEC.md ${PROJECT_SOURCE_DIR}/docs/V1_2_1_SPEC.html
    ${PROJECT_SOURCE_DIR}/docs/V1_2_1_IMPLEMENTATION.md ${PROJECT_SOURCE_DIR}/docs/V1_2_1_QA.md DESTINATION docs)
  install(FILES ${PROJECT_SOURCE_DIR}/docs/AI_USAGE.md
    ${PROJECT_SOURCE_DIR}/docs/v121_balance_results.csv
    ${PROJECT_SOURCE_DIR}/docs/v121_performance_results.csv DESTINATION docs)
  install(FILES ${PROJECT_SOURCE_DIR}/docs/V1_2_2_SPEC.md
    ${PROJECT_SOURCE_DIR}/docs/V1_2_2_IMPLEMENTATION.md
    ${PROJECT_SOURCE_DIR}/docs/V1_2_2_QA.md
    ${PROJECT_SOURCE_DIR}/docs/v122_balance_results.csv
    ${PROJECT_SOURCE_DIR}/docs/v122_performance_results.csv DESTINATION docs)
  install(FILES ${PROJECT_SOURCE_DIR}/docs/screenshots/v122_game.png
    ${PROJECT_SOURCE_DIR}/docs/screenshots/v122_villagers.png
    ${PROJECT_SOURCE_DIR}/docs/screenshots/v122_uniforms.png
    ${PROJECT_SOURCE_DIR}/docs/screenshots/v122_farms.png
    ${PROJECT_SOURCE_DIR}/docs/screenshots/v122_connections.png
    ${PROJECT_SOURCE_DIR}/docs/screenshots/v122_population.png
    ${PROJECT_SOURCE_DIR}/docs/screenshots/v122_market.png
    ${PROJECT_SOURCE_DIR}/docs/screenshots/v122_load.png DESTINATION docs/screenshots)
  install(DIRECTORY ${PROJECT_SOURCE_DIR}/docs/expansion DESTINATION docs)
  install(FILES ${PROJECT_SOURCE_DIR}/docs/screenshots/native_title.png
    ${PROJECT_SOURCE_DIR}/docs/screenshots/native_game.png
    ${PROJECT_SOURCE_DIR}/docs/screenshots/native_villagers.png
    ${PROJECT_SOURCE_DIR}/docs/screenshots/expansion_rotations.png
    ${PROJECT_SOURCE_DIR}/docs/screenshots/fortification_connections.png
    ${PROJECT_SOURCE_DIR}/docs/screenshots/v111_game.png
    ${PROJECT_SOURCE_DIR}/docs/screenshots/v111_connections.png
    ${PROJECT_SOURCE_DIR}/docs/screenshots/v111_rank.png
    ${PROJECT_SOURCE_DIR}/docs/screenshots/v111_market.png DESTINATION docs/screenshots)
  install(FILES ${PROJECT_SOURCE_DIR}/docs/screenshots/v111_population.png DESTINATION docs/screenshots)
  install(FILES ${PROJECT_SOURCE_DIR}/docs/screenshots/v121_game.png
    ${PROJECT_SOURCE_DIR}/docs/screenshots/v121_villagers.png
    ${PROJECT_SOURCE_DIR}/docs/screenshots/v121_connections.png
    ${PROJECT_SOURCE_DIR}/docs/screenshots/v121_rotations.png DESTINATION docs/screenshots)
  if(DEFINED raylib_SOURCE_DIR)
    install(FILES "${raylib_SOURCE_DIR}/LICENSE" DESTINATION licenses RENAME raylib.txt)
    install(FILES "${raylib_SOURCE_DIR}/src/external/glfw/LICENSE.md" DESTINATION licenses RENAME GLFW.txt)
  endif()
  if(CASTLE_BUILD_TESTS)
    add_test(NAME native_smoke_20 COMMAND castle_game --smoke-test --days 20 --seed 42
      --save-dir "${CMAKE_CURRENT_BINARY_DIR}/smoke_saves")
    set_tests_properties(native_smoke_20 PROPERTIES TIMEOUT 600)
    add_test(NAME native_bad_arguments COMMAND castle_game --days abc)
    set_tests_properties(native_bad_arguments PROPERTIES WILL_FAIL TRUE)
  endif()
endif()
