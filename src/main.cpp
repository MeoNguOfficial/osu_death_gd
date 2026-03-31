#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

namespace OsuFail
{
    static bool active = false;
    static float time = 0.0f;
    static float duration = 1.0f;

    static float startMusicPitch = 1.0f;
    static float startSfxPitch   = 1.0f;
    static float endPitch        = 0.3f;

    float easeOut(float t)
    {
        return 1.0f - powf(1.0f - t, 3.0f);
    }

    float getPitch()
    {
        float t = time / duration;
        if (t > 1.0f) t = 1.0f;
        float k = easeOut(t);
        // startPitch -> endPitch (dùng startMusicPitch cho music)
        return startMusicPitch + (endPitch - startMusicPitch) * k;
    }

    void start(float dur)
    {
        auto* fmod = FMODAudioEngine::sharedEngine();
        if (!fmod) return;

        // Lưu pitch hiện tại trước khi modify
        fmod->m_backgroundMusicChannel->getPitch(&startMusicPitch);
        fmod->m_globalChannel->getPitch(&startSfxPitch);

        active   = true;
        time     = 0.0f;
        duration = (dur > 0.05f) ? dur : 1.0f;
    }

    void reset()
    {
        if (!active) return; // tránh reset không cần thiết
        active = false;
        time   = 0.0f;

        auto* fmod = FMODAudioEngine::sharedEngine();
        if (!fmod) return;

        fmod->m_backgroundMusicChannel->setPitch(1.0f);
        fmod->m_globalChannel->setPitch(1.0f);
    }
}

class $modify(MyPlayLayer, PlayLayer)
{
    void update(float dt)
    {
        PlayLayer::update(dt);

        if (!OsuFail::active) return;

        OsuFail::time += dt;

        auto* fmod = FMODAudioEngine::sharedEngine();
        if (!fmod) return;

        float musicPitch = OsuFail::getPitch();

        // SFX pitch cũng kéo xuống tương tự
        float sfxRatio = OsuFail::startSfxPitch / 
                         (OsuFail::startMusicPitch > 0.f ? OsuFail::startMusicPitch : 1.f);
        float sfxPitch = musicPitch * sfxRatio;

        fmod->m_backgroundMusicChannel->setPitch(musicPitch);
        fmod->m_globalChannel->setPitch(sfxPitch);

        if (OsuFail::time >= OsuFail::duration)
        {
            OsuFail::active = false;
            // KHÔNG reset pitch ở đây — giữ nguyên pitch thấp cho đến khi respawn
        }
    }

    bool init(GJGameLevel* level, bool useReplay, bool dontRun)
    {
        OsuFail::reset();
        return PlayLayer::init(level, useReplay, dontRun);
    }

    void resetLevel()
    {
        OsuFail::reset();
        PlayLayer::resetLevel();
    }

    void onQuit()
    {
        OsuFail::reset();
        PlayLayer::onQuit();
    }
};

class $modify(MyPlayerObject, PlayerObject)
{
    void playDeathEffect()
    {
        PlayerObject::playDeathEffect();

        float dur = 1.0f;
        if (auto* mod = Mod::get())
            dur = mod->getSettingValue<double>("fade-speed");

        OsuFail::start(dur);
    }
};