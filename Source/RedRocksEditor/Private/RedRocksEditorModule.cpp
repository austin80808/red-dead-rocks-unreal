#include "HairStrandsEditor.h"
#include "Modules/ModuleManager.h"
#include "RRGroomTranslator.h"

class FRedRocksEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGroomEditor::Get().RegisterHairTranslator<FRRGroomTranslator>();
	}
};

IMPLEMENT_MODULE(FRedRocksEditorModule, RedRocksEditor);
