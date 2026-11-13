#include <QCoreApplication>

extern "C" int LLVMFuzzerInitialize(int *argc, char ***argv)
{
	auto app = new QCoreApplication(*argc, *argv);
	new IkebackCore(app, IkebackCore::Component::CLI, QStringLiteral("Fuzzer"));
	return 0;
}
