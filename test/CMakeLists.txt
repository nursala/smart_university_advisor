cmake_minimum_required(VERSION 3.5)
project(smart_university_advisor_test CXX)

add_executable(${PROJECT_NAME}
    test_main.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../services/AgentLoop.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../services/ToolRegistry.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../services/Tools.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../services/StudentService.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../services/CourseService.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../services/EnrollmentService.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../services/ValidationHelpers.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../services/Base64.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../services/PasswordHasher.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../services/JwtService.cc)
target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../services/AuthorizationService.cc
    ${CMAKE_CURRENT_SOURCE_DIR}/../services/AcademicRules.cc
)

target_include_directories(${PROJECT_NAME}
                           PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)

# ##############################################################################
# If you include the drogon source code locally in your project, use this method
# to add drogon
# target_link_libraries(${PROJECT_NAME} PRIVATE drogon)
#
# and comment out the following lines
target_link_libraries(${PROJECT_NAME} PRIVATE Drogon::Drogon)

find_package(OpenSSL REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE OpenSSL::Crypto)

ParseAndAddDrogonTests(${PROJECT_NAME})
