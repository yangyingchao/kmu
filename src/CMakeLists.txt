if(PYTHON)
  add_executable(ckmu main.c util.c)
  install(TARGETS ckmu
    RUNTIME DESTINATION bin)
  install(PROGRAMS kmu.py DESTINATION bin
    RENAME kmu)
else(PYTHON)
  add_executable(kmu main.c util.c)
  install(TARGETS kmu
    RUNTIME DESTINATION bin)
endif(PYTHON)

