SET(INC_INSTALL_DIR ${CMAKE_INSTALL_INCLUDEDIR}/caer-sdk)
INSTALL(FILES module.h mainloop.h utils.h buffers.h DESTINATION ${INC_INSTALL_DIR})
INSTALL(DIRECTORY cross DESTINATION ${INC_INSTALL_DIR} FILES_MATCHING PATTERN "*.h")
INSTALL(DIRECTORY sshs DESTINATION ${INC_INSTALL_DIR} FILES_MATCHING PATTERN "*.h")
INSTALL(DIRECTORY sshs DESTINATION ${INC_INSTALL_DIR} FILES_MATCHING PATTERN "*.hpp")
