SUBDIRS = .

AM_CPPFLAGS = -I.
bin_PROGRAMS=answeringmachine
answeringmachine_SOURCES = \
	answeringmachine.cpp  ClientContext.cpp  Deamonize.cpp  logging.cpp   signalhandler.cpp \
	CallSession.cpp       ConfigClient.cpp   Logger.cpp     makecall.cpp  processanswer.cpp

LDFLAGS = -L/usr/local/lib
answeringmachine_LDADD = \
	-L/usr/local/lib 
