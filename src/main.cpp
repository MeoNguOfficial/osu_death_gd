#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

// =============================================
// OSU DEATH EFFECT STATE
// =============================================
namespace OsuFail
{
    static bool  active   = false;
    static float time     = 0.0f;
    static float duration = 1.0f;

    // Giá trị gốc lúc bắt đầu effect
    static float startMusicPitch  = 1.0f;
    static float startSfxPitch    = 1.0f;
    static float startMusicVolume = 1.0f;
    static float startSfxVolume   = 1.0f;

    // Target cuối (pitch+volume xuống thấp để tạo méo)
    static constexpr float END_PITCH  = 0.2f;
    static constexpr float END_VOLUME = 0.0f;

    // Easing: ease-out cubic — nhanh lúc đầu, chậm dần
    float easeOut(float t)
    {
        return 1.0f - powf(1.0f - t, 3.0f);
    }

    void applyEffect()
    {
        auto* fmod = FMODAudioEngine::sharedEngine();
        if (!fmod) return;

        float t = std::min(time / duration, 1.0f);
        float k = easeOut(t);

        // Pitch giảm dần (tạo hiệu ứng "chậm lại + méo")
        float musicPitch = startMusicPitch  - (startMusicPitch  - END_PITCH)  * k;
        float sfxPitch   = startSfxPitch    - (startSfxPitch    - END_PITCH)  * k;

        // Volume giảm dần về 0
        float musicVol   = startMusicVolume - (startMusicVolume - END_VOLUME) * k;
        float sfxVol     = startSfxVolume   - (startSfxVolume   - END_VOLUME) * k;

        fmod->m_backgroundMusicChannel->setPitch(musicPitch);
        fmod->m_globalChannel->setPitch(sfxPitch);

        fmod->m_backgroundMusicChannel->setVolume(musicVol);
        fmod->m_globalChannel->setVolume(sfxVol);
    }

    void start(float dur)
    {
        if (active) return; // không restart nếu đang chạy

        auto* fmod = FMODAudioEngine::sharedEngine();
        if (!fmod) return;

        // Lưu pitch + volume hiện tại trước khi modify
        fmod->m_backgroundMusicChannel->getPitch(&startMusicPitch);
        fmod->m_globalChannel->getPitch(&startSfxPitch);

        startMusicVolume = fmod->m_musicVolume;
        startSfxVolume   = fmod->m_sfxVolume;

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

        // Restore pitch về 1.0 — volume sẽ do GD tự quản lý qua resetLevel
        fmod->m_backgroundMusicChannel->setPitch(1.0f);
        fmod->m_globalChannel->setPitch(1.0f);

        // Restore volume về giá trị setting của game (không hardcode 1.0)
        fmod->m_backgroundMusicChannel->setVolume(fmod->m_musicVolume);
        fmod->m_globalChannel->setVolume(fmod->m_sfxVolume);
    }
}

// =============================================
// PLAYLAYER HOOK
// =============================================
class $modify(MyPlayLayer, PlayLayer)
{
    // Tick effect mỗi frame
    void update(float dt)
    {
        PlayLayer::update(dt);

        if (!OsuFail::active) return;

        OsuFail::time += dt;
        OsuFail::applyEffect();

        if (OsuFail::time >= OsuFail::duration)
            OsuFail::active = false;
        // pitch+volume giữ ở mức thấp cho đến khi resetLevel/onQuit restore
    }

    // ✅ Trigger death effect — bind đầy đủ trên Windows (0x3b39d0)
    void destroyPlayer(PlayerObject* player, GameObject* object)
    {
        PlayLayer::destroyPlayer(player, object);

        // Chỉ trigger cho player1 hoặc player2 thật
        if (player != m_player1 && player != m_player2) return;

        // Không trigger nếu level đã complete
        if (m_hasCompletedLevel) return;

        // Không trigger lại nếu effect đang chạy
        if (OsuFail::active) return;

        float dur = 1.0f;
        if (auto* mod = Mod::get())
            dur = static_cast<float>(mod->getSettingValue<double>("fade-speed"));

        OsuFail::start(dur);
    }

    // Reset khi respawn
    void resetLevel()
    {
        OsuFail::reset();
        PlayLayer::resetLevel();
    }

    // Reset khi vào level
    bool init(GJGameLevel* level, bool useReplay, bool dontRun)
    {
        OsuFail::reset();
        return PlayLayer::init(level, useReplay, dontRun);
    }

    // Reset khi thoát
    void onQuit()
    {
        OsuFail::reset();
        PlayLayer::onQuit();
    }
};