#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

namespace OsuFail
{
    static bool active    = false;
    static float time     = 0.0f;
    static float duration = 1.0f;

    static float startMusicPitch = 1.0f;
    static float startSfxPitch   = 1.0f;
    static const float endPitch  = 0.3f;

    float easeOut(float t)
    {
        return 1.0f - powf(1.0f - t, 3.0f);
    }

    void applyPitch()
    {
        auto* fmod = FMODAudioEngine::sharedEngine();
        if (!fmod) return;

        float t = std::min(time / duration, 1.0f);
        float k = easeOut(t);

        fmod->m_backgroundMusicChannel->setPitch(startMusicPitch + (endPitch - startMusicPitch) * k);
        fmod->m_globalChannel->setPitch(startSfxPitch + (endPitch - startSfxPitch) * k);
    }

    void start(float dur)
    {
        if (active) return;

        auto* fmod = FMODAudioEngine::sharedEngine();
        if (!fmod) return;

        fmod->m_backgroundMusicChannel->getPitch(&startMusicPitch);
        fmod->m_globalChannel->getPitch(&startSfxPitch);

        active   = true;
        time     = 0.0f;
        duration = (dur > 0.05f) ? dur : 1.0f;
    }

    void reset()
    {
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
        OsuFail::applyPitch();

        if (OsuFail::time >= OsuFail::duration)
            OsuFail::active = false;
    }

    // ✅ Hook đúng — có bind trên Windows (win 0x3b39d0)
    void destroyPlayer(PlayerObject* player, GameObject* object)
    {
        PlayLayer::destroyPlayer(player, object);

        // Chỉ trigger cho player chính, tránh dual-mode player2 không quan trọng
        if (player != m_player1 && player != m_player2) return;

        // Không trigger lại nếu đang chạy
        if (OsuFail::active) return;

        float dur = 1.0f;
        if (auto* mod = Mod::get())
            dur = static_cast<float>(mod->getSettingValue<double>("fade-speed"));

        OsuFail::start(dur);
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