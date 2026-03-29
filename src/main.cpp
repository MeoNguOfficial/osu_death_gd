#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/FMODAudioEngine.hpp>

using namespace geode::prelude;

// Hook PlayerObject to catch the death event
class $modify(MyPlayer, PlayerObject) {
    // GD 2.208 uses one bool parameter for playerDestroyed
    void playerDestroyed(bool p0) { 
        PlayerObject::playerDestroyed(p0);

        auto pitchVal = static_cast<float>(Mod::get()->getSettingValue<double>("death-pitch"));
        auto engine = FMODAudioEngine::sharedEngine();
        
        if (engine && engine->m_pMusicChannel) {
            // Instantly apply the pitch down effect
            engine->m_pMusicChannel->setPitch(pitchVal);
        }
    }
};

// Hook PlayLayer to reset music state
class $modify(MyPlayLayer, PlayLayer) {
    // Reset pitch when restarting the level
    void resetLevel() {
        PlayLayer::resetLevel();
        auto engine = FMODAudioEngine::sharedEngine();
        if (engine && engine->m_pMusicChannel) {
            engine->m_pMusicChannel->setPitch(1.0f);
        }
    }

    // Reset pitch when leaving to the menu
    void onQuit() {
        PlayLayer::onQuit();
        auto engine = FMODAudioEngine::sharedEngine();
        if (engine && engine->m_pMusicChannel) {
            engine->m_pMusicChannel->setPitch(1.0f);
        }
    }
};
