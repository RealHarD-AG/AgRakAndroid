set(LUAJIT_DIST_DIR "${CMAKE_BINARY_DIR}/luajit")
set(LUAJIT_SRC_DIR "${CMAKE_SOURCE_DIR}/deps/luajit/src")

if(NOT EXISTS "${LUAJIT_SRC_DIR}/lua.h")
	message(FATAL_ERROR "LuaJIT headers missing at ${LUAJIT_SRC_DIR}. Expected deps/luajit (v2.1).")
endif()

if(WIN32)
	set(_lj_dll "${LUAJIT_DIST_DIR}/lua51.dll")
	set(_lj_implib "${LUAJIT_DIST_DIR}/liblua51.dll.a")
	if(NOT EXISTS "${_lj_dll}" OR NOT EXISTS "${_lj_implib}")
		message(FATAL_ERROR
			"LuaJIT not built yet.\n"
			"Run: ./tools/build_luajit.sh <win32|win64> ${LUAJIT_DIST_DIR}\n"
			"(build.sh does this automatically)")
	endif()
	add_library(luajit_shared SHARED IMPORTED GLOBAL)
	set_target_properties(luajit_shared PROPERTIES
		IMPORTED_LOCATION "${_lj_dll}"
		IMPORTED_IMPLIB "${_lj_implib}"
		INTERFACE_INCLUDE_DIRECTORIES "${LUAJIT_SRC_DIR}"
	)
else()
	set(_lj_so "${LUAJIT_DIST_DIR}/libluajit-5.1.so.2")
	if(NOT EXISTS "${_lj_so}")
		set(_lj_so "${LUAJIT_DIST_DIR}/libluajit.so")
	endif()
	if(NOT EXISTS "${_lj_so}")
		message(FATAL_ERROR
			"LuaJIT not built yet.\n"
			"Run: ./tools/build_luajit.sh linux ${LUAJIT_DIST_DIR}\n"
			"(build.sh does this automatically)")
	endif()
	add_library(luajit_shared SHARED IMPORTED GLOBAL)
	set_target_properties(luajit_shared PROPERTIES
		IMPORTED_LOCATION "${_lj_so}"
		INTERFACE_INCLUDE_DIRECTORIES "${LUAJIT_SRC_DIR}"
		IMPORTED_NO_SONAME TRUE
	)
endif()

message(STATUS "LuaJIT headers: ${LUAJIT_SRC_DIR}")
message(STATUS "LuaJIT dist:    ${LUAJIT_DIST_DIR}")
