add_executable(kmu main.c util.c)
install(TARGETS kmu
RUNTIME DESTINATION bin)