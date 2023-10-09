#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

#include "../ExternalPlugin.h"
#include "../JuceHeader.h"

namespace Pedalboard {
class Current : public ExternalPlugin<juce::VST3PluginFormat> {
public:
  Current(std::string inPluginPath)
      : ExternalPlugin<juce::VST3PluginFormat>(inPluginPath){};

  void loadMinimalAudioCurrentPreset(const std::string &inPresetPath,
                                     const std::string &inPresetName,
                                     const std::string &inPresetUUID,
                                     const std::string &inPresetPackName) {
    auto preset_file = juce::File(inPresetPath);

    if (!preset_file.exists()) {
      throw std::invalid_argument("Preset file does not exists: " +
                                  inPresetPath);
      return;
    }

    std::unique_ptr<juce::XmlElement> presetXml(juce::parseXML(preset_file));

    if (!presetXml) {
      throw std::runtime_error("Preset file corrupted: " + inPresetPath);
      return;
    }

    // Get Current's current state
    juce::MemoryBlock data;
    pluginInstance->getStateInformation(data);

    // Remove the vst3 stuff arround it
    std::unique_ptr<juce::XmlElement> vst3State(
        juce::AudioProcessor::getXmlFromBinary(data.getData(), data.getSize()));

    if (!vst3State) {
      throw std::runtime_error("Unexpected VST3 state structure.");
      return;
    }

    auto *vst3State_child = vst3State->getChildByName("IComponent");

    if (!vst3State_child) {
      throw std::runtime_error("Unexpected VST3 state structure (IComponent).");
      return;
    }

    // Get actual Current current state
    juce::MemoryBlock mem;
    mem.fromBase64Encoding(vst3State_child->getAllSubText());
    std::unique_ptr<juce::XmlElement> currentState(
        juce::AudioProcessor::getXmlFromBinary(mem.getData(), mem.getSize()));

    if (!currentState) {
      throw std::runtime_error("Could not read Current state.");
      return;
    }

    juce::XmlElement *sessionInfoXml =
        currentState->getChildByName("SessionInfo");
    juce::XmlElement *sessionPresetMeta =
        currentState->getChildByName("session_preset_meta");

    if (!sessionInfoXml) {
      throw std::runtime_error("Error when loading Minimal Audio Current "
                               "preset: No sessionInfoXml");
    }

    if (!sessionPresetMeta) {
      throw std::runtime_error("Error when loading Minimal Audio Current "
                               "preset: No sessionPresetMeta");
    }

    currentState->removeChildElement(sessionInfoXml, false);
    currentState->removeChildElement(sessionPresetMeta, false);

    // Modify session_preset_meta
    sessionPresetMeta->setAttribute("preset_file_path", inPresetPath);
    sessionPresetMeta->setAttribute("preset_file_name", inPresetName);
    sessionPresetMeta->setAttribute("preset_file_pack", inPresetPackName);
    sessionPresetMeta->setAttribute("preset_file_uid", inPresetUUID);

    // Remove tag from preset Meta
    presetXml->getChildByName("Meta")->removeAttribute("TAGS");

    presetXml->addChildElement(sessionInfoXml);
    presetXml->addChildElement(sessionPresetMeta);

    juce::MemoryBlock newState;
    juce::AudioProcessor::copyXmlToBinary(*presetXml, newState);

    // Wrap new state into VST3 specific stuffs
    juce::XmlElement finalXML("VST3PluginState");
    finalXML.createNewChildElement("IComponent")
        ->addTextElement(newState.toBase64Encoding());

    juce::MemoryBlock finalState;
    juce::AudioProcessor::copyXmlToBinary(finalXML, finalState);

    pluginInstance->setStateInformation(finalState.getData(),
                                        finalState.getSize());
  }

  // To force Current to be recognized as an instrument.
  bool acceptsAudioInput() override { return false; }
};

inline void init_current(py::module &m) {
  py::class_<Current, ExternalPlugin<juce::VST3PluginFormat>,
             std::shared_ptr<Current>>(
      m, "Current", "Load Current, a synthesizer from Minimal Audio")
      .def(py::init([](std::string inPluginPath) {
             if (inPluginPath.empty()) {
#if JUCE_WINDOWS
               inPluginPath =
                   "C:\\Program Files\\Common Files\\VST3\\Current.vst3\\Contents\\x86_64-win\\Current.vst3";
#elif JUCE_MAC
               inPluginPath = "/Library/Audio/Plug-Ins/VST3/Current.vst3";
#else
               static_assert("You should be on either Windows or Mac.");
#endif
             }

             auto plugin = std::make_unique<Current>(inPluginPath);
             return plugin;
           }),
           py::arg("plugin_path") = "")
      .def(
          "load_ma_current_preset",
          [](std::shared_ptr<Current> self, const std::string &inPresetFilePath,
             const std::string &inPresetName, const std::string &inPresetUUID,
             const std::string &inPresetPackName) {
            self->loadMinimalAudioCurrentPreset(inPresetFilePath, inPresetName,
                                                inPresetUUID, inPresetPackName);
          },
          "Load a Current preset", py::arg("preset_file_path"),
          py::arg("preset_display_name") = "", py::arg("preset_uuid") = "",
          py::arg("preset_pack_name") = "");
}

}; // namespace Pedalboard