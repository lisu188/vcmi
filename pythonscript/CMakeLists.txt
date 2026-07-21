include_directories(${CMAKE_HOME_DIRECTORY} ${CMAKE_HOME_DIRECTORY}/include ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_HOME_DIRECTORY}/lib)

set(pythonlib_SRCS
		StdInc.cpp
		PythonJson.cpp
		PythonScriptInstance.cpp
		PythonScriptPool.cpp
		PythonContext.cpp
		PythonModule.cpp

		api/VcmiModule.cpp
)

set(pythonlib_HDRS
		StdInc.h
		PythonJson.h
		PythonScriptInstance.h
		PythonScriptPool.h
		PythonContext.h
		PythonModule.h
)

assign_source_group(${pythonlib_SRCS} ${pythonlib_HDRS})

add_library(vcmiPython OBJECT ${pythonlib_SRCS} ${pythonlib_HDRS})
target_compile_definitions(vcmiPython PRIVATE VCMI_DLL=1)
target_link_libraries(vcmiPython Boost::boost pybind11::embed vcmiMain)

vcmi_set_output_dir(vcmiPython "scripting")
enable_pch(vcmiPython)
