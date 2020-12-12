#include "engines/metaengine.h"
#include "base/plugins.h"
#include "common/md5.h"
#include "common/memstream.h"
#include "common/savefile.h"
#include "common/str-array.h"
#include "common/system.h"
#include "graphics/surface.h"
#include "common/config-manager.h"
#include "common/file.h"

#include "kom/kom.h"

class KomMetaEngine : public MetaEngine {
private:
	Common::String findFileByGameId(const Common::String &gameId) const;
public:
	const char* getName() const override {
		return "kom";
	}

	Common::Error createInstance(OSystem *syst, Engine **engine) const override;
};

Common::Error KomMetaEngine::createInstance(OSystem *syst, Engine **engine) const {
	Common::FSNode dir(ConfMan.get("path"));

	// Unable to locate game data
	if (!(dir.getChild("thidney.dsk").exists() || dir.getChild("shahron.dsk").exists())) {
		warning("KOM: unable to locate game data at path '%s'", dir.getPath().c_str());
		return Common::kNoGameDataFoundError;
	}

	if (engine == NULL) {
		return Common::kUnknownError;
	}

	*engine = new Kom::KomEngine(syst);
	return Common::kNoError;
}

#if PLUGIN_ENABLED_DYNAMIC(KOM)
	REGISTER_PLUGIN_DYNAMIC(KOM, PLUGIN_TYPE_ENGINE, KomMetaEngine);
#else
	REGISTER_PLUGIN_STATIC(KOM, PLUGIN_TYPE_ENGINE, KomMetaEngine);
#endif
