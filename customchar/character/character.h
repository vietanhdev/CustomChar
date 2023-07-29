#ifndef CUSTOMCHAR_CHARACTER_CHARACTER_H_
#define CUSTOMCHAR_CHARACTER_CHARACTER_H_

#include "customchar/audio/speech_recognizer.h"
#include "customchar/audio/voice_recorder.h"
#include "customchar/audio/voice_synthesizer.h"
#include "customchar/common/common.h"
#include "customchar/common/helpers.h"
#include "customchar/executors/plugin_executor.h"
#include "customchar/llm/llm.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <functional>
#include <regex>
#include <string>
#include <thread>
#include <vector>

namespace CC {

class Character {
 private:
  CCParams params_;
  std::shared_ptr<SpeechRecognizer> speech_recognizer_;
  std::shared_ptr<VoiceRecorder> voice_recoder_;
  std::shared_ptr<VoiceSynthesizer> voice_synthesizer_;
  std::shared_ptr<LLM> llm_;
  std::shared_ptr<executors::PluginExecutor> plugin_executor_;

  std::function<void(std::string)> on_user_message_;
  std::function<void(std::string)> on_bot_message_;

 public:
  /// @brief Constructor
  /// @param init_params
  Character(CCParams init_params);

  /// @brief Set on user message callback
  /// @param on_user_message_
  void SetOnUserMessage(std::function<void(std::string)> on_user_message_);

  /// @brief Set on bot message callback
  /// @param on_bot_message_
  void SetOnBotMessage(std::function<void(std::string)> on_bot_message_);

  /// @brief Run character in a loop
  void Run();
};

}  // namespace CC

#endif  // CUSTOMCHAR_CHARACTER_CHARACTER_H_
