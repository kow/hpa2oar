# -*- cmake -*-

project(hpa2oar)

include(00-Common)
include(LLCommon)
include(LLPrimitive)
include(LLXML)
include(LLMessage)
include(LLMath)
include(LLVFS)
include(Linking)

include_directories(
    ${LLCOMMON_INCLUDE_DIRS}
    ${LLXML_INCLUDE_DIRS}
	${LLMATH_INCLUDE_DIRS}
	${LLMESSAGE_INCLUDE_DIRS}
	${LLPRIMITIVE_INCLUDE_DIRS}
    ${LLVFS_INCLUDE_DIRS}
    )

set(HPA2OAR_SOURCE_FILES

    hpa2oar.cpp
    )

set(HPA2OAR_HEADER_FILES
    CMakeLists.txt

    hpa2oar.h
    )

set_source_files_properties(${HPA2OAR_HEADER_FILES}
                            PROPERTIES HEADER_FILE_ONLY TRUE)

list(APPEND HPA2OAR_SOURCE_FILES ${HPA2OAR_HEADER_FILES})
   
add_executable(hpa2oar ${HPA2OAR_SOURCE_FILES})
add_dependencies(hpa2oar llcommon llvfs llxml llmath llmessage llprimitive)

target_link_libraries(hpa2oar
	${LLCOMMON_LIBRARIES}
    ${LLXML_LIBRARIES}
	${LLMATH_LIBRARIES}
	${LLPRIMITIVE_LIBRARIES}
	${LLMESSAGE_LIBRARIES}
    ${LLVFS_LIBRARIES}
    ${viewer_LIBRARIES}
    ${BOOST_PROGRAM_OPTIONS_LIBRARY}
    ${BOOST_REGEX_LIBRARY}
	llmath #Wait, what? Why do I need to do this?
    )

add_custom_command(
      TARGET copy_testscripts
      COMMAND ${CMAKE_COMMAND} -E copy ${SRC} ${DST}
      )

if (WINDOWS)
    set_target_properties(hpa2oar
        PROPERTIES 
        LINK_FLAGS "/NODEFAULTLIB:LIBCMT"
        LINK_FLAGS_DEBUG "/NODEFAULTLIB:\"LIBCMT;LIBCMTD;MSVCRT\""
        )
endif (WINDOWS)

if (LINUX)
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--as-needed")

    set(viewer_LIBRARIES
        Xinerama
        )

	add_custom_command(
	  TARGET hpa2oar
	  COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/test.sh" "${CMAKE_CURRENT_BINARY_DIR}/test.sh"
	  )
endif (LINUX)
