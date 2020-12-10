LIB_APP_SRC:=lib_app/utils.cpp\
	     lib_app/convert.cpp\
	     lib_app/BufPool.cpp\
	     lib_app/PixMapBufPool.cpp\
         lib_app/AllocatorTracker.cpp\
         lib_app/YuvIO.cpp\
         lib_app/JsonFile.cpp\




ifneq ($(ENABLE_HIGH_DYNAMIC_RANGE),0)
  LIB_APP_SRC+=lib_app/JsonFile.cpp
endif

ifeq ($(findstring mingw,$(TARGET)),mingw)
  LIB_APP_SRC+=lib_app/plateform_windows.cpp
else
  LIB_APP_SRC+=lib_app/console_linux.cpp
  LIB_APP_SRC+=lib_app/plateform_linux.cpp
endif

