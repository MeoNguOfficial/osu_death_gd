#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>

using namespace geode::prelude;

// =========================
// STATE
// =========================
namespace OsuFail {
    static bool active = false;
    static float time = 0.0f;
    static float duration = 1.0f;

    static float startPitch = 1.0f;
    static float endPitch = 0.3f;

    // easing (smooth như osu)
    float easeOut(float t) {
        return 1.0f - powf(1.0f - t, 3.0f);
    }

    float getPitch() {
        float t = time / duration;
        if (t > 1.0f) t = 1.0f;

        float k = easeOut(t);
        return startPitch + (endPitch - startPitch) * k;
    }

    void start(float dur) {
        active = true;
        time = 0.0f;
        duration = dur > 0.05f ? dur : 1.0f;
    }

    void reset() {
        active = false;
        time = 0.0f;

        // reset pitch global
        if (auto fmod = FMODAudioEngine::sharedEngine()) {
            if (fmod->m_system) {
                FMOD::ChannelGroup* master = nullptr;
                fmod->m_system->getMasterChannelGroup(&master);
                if (master) master->setPitch(1.0f);
            }
        }
    }
}

// =========================
// APPLY EFFECT (SAFE LAYER)
// =========================
class $modify(MyPlayLayer, PlayLayer) {

    void update(float dt) {
        PlayLayer::update(dt);

        if (!OsuFail::active) return;

        OsuFail::time += dt;

        auto fmod = FMODAudioEngine::sharedEngine();
        if (!fmod || !fmod->m_system) return;

        FMOD::ChannelGroup* master = nullptr;
        fmod->m_system->getMasterChannelGroup(&master);
        if (!master) return;

        float pitch = OsuFail::getPitch();
        master->setPitch(pitch);

        // stop effect khi xong
        if (OsuFail::time >= OsuFail::duration) {
            OsuFail::active = false;
        }
    }

    bool init(GJGameLevel* level, bool useReplay, bool dontRun) {
        OsuFail::reset();
        return PlayLayer::init(level, useReplay, dontRun);
    }

    void resetLevel() {
        OsuFail::reset();
        PlayLayer::resetLevel();
    }

    void onQuit() {
        OsuFail::reset();
        PlayLayer::onQuit();
    }
};

// =========================
// TRIGGER (PLAYER DIE)
// =========================
class $modify(MyPlayerObject, PlayerObject) {
    void playDeathEffect() {
        PlayerObject::playDeathEffect();

        // trigger fail effect
        float dur = 1.0f;
        if (auto mod = Mod::get()) {
            dur = mod->getSettingValue<double>("fade-speed");
        }

        OsuFail::start(dur);
    }
};
