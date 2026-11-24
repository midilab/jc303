// CLAP preset discovery: JC303's presets listed to a host without a plugin instance.
//
// clap-juce-extensions serves no preset-discovery factory of its own, but with
// CLAP_SUPPORTS_CUSTOM_FACTORY (CMakeLists.txt) its clap_entry asks clapJuceExtensionCustomFactory
// for any factory id it does not serve. This answers for preset discovery.
//
// Each bank folder is a file location: the factory bank as factory content, every other folder as
// user content, each one a sound pack named after it. The host crawls them for .json files and
// asks get_metadata about each. The factory folder is seeded first if it is missing, as the plugin
// does on first use, so a host that indexes before any instance has run still finds the factory
// sounds; the disk stays the truth, so an edited factory preset is listed as edited. Loading is
// JC303::presetLoadFromLocation, through the clap.preset-load extension the wrapper offers.
//
// Everything here runs on the host's indexing thread and touches files only: never the loaded
// bank, which belongs to the plugin instances.

#include <JuceHeader.h>

#include <clap/clap.h>

#include "Presets.h"

#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace
{
    const clap_preset_discovery_provider_descriptor providerDescriptor {
        CLAP_VERSION_INIT,
        "co.midilab.JC303.presets",
        "JC303 presets",
        "midilab",
    };

    struct Bank
    {
        juce::File dir;
        std::string id;   // the sound pack's id: the bank's folder name
        bool factory;
    };

    struct Provider
    {
        clap_preset_discovery_provider clap {};
        const clap_preset_discovery_indexer* indexer = nullptr;
        std::vector<Bank> banks;   // declared by init, so get_metadata can say which a file is in
    };

    Provider& self (const clap_preset_discovery_provider* provider)
    {
        return *static_cast<Provider*> (provider->provider_data);
    }

    bool CLAP_ABI providerInit (const clap_preset_discovery_provider* provider)
    {
        auto& p = self (provider);
        seedFactoryPresetsIfMissing();

        const clap_preset_discovery_filetype json { "JC303 preset", "A JC303 sound", "json" };
        if (! p.indexer->declare_filetype (p.indexer, &json))
            return false;

        for (const auto& bank : presetBanks())
        {
            const juce::File dir = presetBankDirectory (bank);
            if (! dir.isDirectory())
                continue;

            const bool factory = bank == "factory";
            const uint32_t flags = factory ? CLAP_PRESET_DISCOVERY_IS_FACTORY_CONTENT
                                           : CLAP_PRESET_DISCOVERY_IS_USER_CONTENT;
            const std::string id = bank.toStdString();
            const std::string name = factory ? std::string ("Factory") : id;
            const std::string path = dir.getFullPathName().toStdString();

            const clap_preset_discovery_soundpack pack {
                flags, id.c_str(), name.c_str(), nullptr, nullptr, "midilab", nullptr, CLAP_TIMESTAMP_UNKNOWN,
            };
            p.indexer->declare_soundpack (p.indexer, &pack);

            const clap_preset_discovery_location location {
                flags, name.c_str(), CLAP_PRESET_DISCOVERY_LOCATION_FILE, path.c_str(),
            };
            if (! p.indexer->declare_location (p.indexer, &location))
                return false;

            p.banks.push_back ({ dir, id, factory });
        }
        return true;
    }

    void CLAP_ABI providerDestroy (const clap_preset_discovery_provider* provider)
    {
        delete &self (provider);
    }

    bool CLAP_ABI providerGetMetadata (const clap_preset_discovery_provider* provider,
                                       uint32_t locationKind,
                                       const char* location,
                                       const clap_preset_discovery_metadata_receiver* receiver)
    {
        if (locationKind != CLAP_PRESET_DISCOVERY_LOCATION_FILE || location == nullptr)
            return false;

        const juce::File file (juce::String::fromUTF8 (location));
        FactoryPreset preset;
        if (! readPresetFile (file, preset))
        {
            receiver->on_error (receiver, 0, "not a JC303 preset: no JSON object in the file");
            return false;
        }

        // One preset per file, so no load key.
        const std::string name = preset.name.toStdString();
        if (! receiver->begin_preset (receiver, name.c_str(), nullptr))
            return true;

        const clap_universal_plugin_id plugin { "clap", JC303_CLAP_ID };
        receiver->add_plugin_id (receiver, &plugin);

        for (const auto& bank : self (provider).banks)
            if (file.getParentDirectory() == bank.dir)
            {
                receiver->set_soundpack_id (receiver, bank.id.c_str());
                receiver->set_flags (receiver, bank.factory ? CLAP_PRESET_DISCOVERY_IS_FACTORY_CONTENT
                                                            : CLAP_PRESET_DISCOVERY_IS_USER_CONTENT);
                break;
            }
        return true;
    }

    const void* CLAP_ABI providerGetExtension (const clap_preset_discovery_provider*, const char*)
    {
        return nullptr;
    }

    uint32_t CLAP_ABI factoryCount (const clap_preset_discovery_factory*)
    {
        return 1;
    }

    const clap_preset_discovery_provider_descriptor* CLAP_ABI factoryGetDescriptor (
        const clap_preset_discovery_factory*, uint32_t index)
    {
        return index == 0 ? &providerDescriptor : nullptr;
    }

    const clap_preset_discovery_provider* CLAP_ABI factoryCreate (const clap_preset_discovery_factory*,
                                                                  const clap_preset_discovery_indexer* indexer,
                                                                  const char* providerId)
    {
        if (providerId == nullptr || std::strcmp (providerId, providerDescriptor.id) != 0)
            return nullptr;

        auto* p = new Provider;
        p->indexer = indexer;
        p->clap = { &providerDescriptor, p, providerInit, providerDestroy, providerGetMetadata,
                    providerGetExtension };
        return &p->clap;
    }

    const clap_preset_discovery_factory presetDiscoveryFactory { factoryCount, factoryGetDescriptor,
                                                                 factoryCreate };
}

// Declared by the wrapper, which calls it; declared here too, as the definition's own.
extern const void* JUCE_CALLTYPE clapJuceExtensionCustomFactory (const char* factoryId);

const void* JUCE_CALLTYPE clapJuceExtensionCustomFactory (const char* factoryId)
{
    if (factoryId != nullptr
        && (std::strcmp (factoryId, CLAP_PRESET_DISCOVERY_FACTORY_ID) == 0
            || std::strcmp (factoryId, CLAP_PRESET_DISCOVERY_FACTORY_ID_COMPAT) == 0))
        return &presetDiscoveryFactory;
    return nullptr;
}
