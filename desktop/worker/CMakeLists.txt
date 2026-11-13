include(BuildIkebackApplication)

build_ikeback_application(ikeback-worker
	NAME Worker
	SOURCES
	src/FeatureWorkerManagerConnection.cpp
	src/FeatureWorkerManagerConnection.h
	src/main.cpp
	src/IkebackWorker.cpp
	src/IkebackWorker.h
	)
