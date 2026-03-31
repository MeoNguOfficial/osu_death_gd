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

    // Giá trị gốc để khôi phục chính xác
    static float startMusicPitch  = 1.0f;
    static float startSfxPitch    = 1.0f;
    static float startMusicVolume = 1.0f;
    static float startSfxVolume   = 1.0f;

    static constexpr float END_PITCH  = 0.15f; // Giảm sâu hơn để nghe rõ hiệu ứng "méo"
    static constexpr float END_VOLUME = 0.0f;

    float easeOut(float t)
    {
        return 1.0f - powf(1.0f - t, 3.0f);
    }

    void applyEffect()
    {
        auto* fmod = FMODAudioEngine::sharedEngine();
        // Kiểm tra an toàn: Đảm bảo engine và các channel tồn tại
        if (!fmod || !fmod->m_backgroundMusicChannel || !fmod->m_globalChannel) return;

        float t = std::min(time / duration, 1.0f);
        float k = easeOut(t);

        // Tính toán giá trị mới
        float musicPitch = startMusicPitch - (startMusicPitch - END_PITCH) * k;
        float sfxPitch   = startSfxPitch   - (startSfxPitch   - END_PITCH) * k;
        float musicVol   = startMusicVolume - (startMusicVolume - END_VOLUME) * k;
        float sfxVol     = startSfxVolume   - (startSfxVolume   - END_VOLUME) * k;

        // Áp dụng trực tiếp vào Channel qua FMOD API (định nghĩa trong FMOD.bro)
        fmod->m_backgroundMusicChannel->setPitch(musicPitch);
        fmod->m_globalChannel->setPitch(sfxPitch);

        fmod->m_backgroundMusicChannel->setVolume(musicVol);
        fmod->m_globalChannel->setVolume(sfxVol);
    }

    void start(float dur)
    {
        if (active) return;

        auto* fmod = FMODAudioEngine::sharedEngine();
        if (!fmod || !fmod->m_backgroundMusicChannel || !fmod->m_globalChannel) return;

        // Lấy Pitch/Volume thực tế tại thời điểm chết thay vì dùng biến m_musicVolume
        // Điều này giúp hiệu ứng mượt mà kể cả khi người dùng đang chỉnh volume trong game
        fmod->m_backgroundMusicChannel->getPitch(&startMusicPitch);
        fmod->m_globalChannel->getPitch(&startSfxPitch);
        
        fmod->m_backgroundMusicChannel->getVolume(&startMusicVolume);
        fmod->m_globalChannel->getVolume(&startSfxVolume);

        active   = true;
        time     = 0.0f;
        duration = (dur > 0.01f) ? dur : 1.0f;
    }

    void reset()
    {
        if (!active && time == 0.0f) return;

        active = false;
        time   = 0.0f;

        auto* fmod = FMODAudioEngine::sharedEngine();
        if (!fmod) return;

        // Khôi phục Pitch về mặc định
        if (fmod->m_backgroundMusicChannel) fmod->m_backgroundMusicChannel->setPitch(1.0f);
        if (fmod->m_globalChannel) fmod->m_globalChannel->setPitch(1.0f);

        // Khôi phục Volume về cài đặt của người dùng (đọc từ biến m_musicVolume của engine)
        if (fmod->m_backgroundMusicChannel) fmod->m_backgroundMusicChannel->setVolume(fmod->m_musicVolume);
        if (fmod->m_globalChannel) fmod->m_globalChannel->setVolume(fmod->m_sfxVolume);
    }
}

// =============================================
// PLAYLAYER HOOK
// =============================================
class $modify(MyPlayLayer, PlayLayer)
{
    void update(float dt)
    {
        PlayLayer::update(dt);

        if (OsuFail::active) {
            OsuFail::time += dt;
            OsuFail::applyEffect();

            if (OsuFail::time >= OsuFail::duration) {
                OsuFail::active = false;
            }
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* object)
    {
        PlayLayer::destroyPlayer(player, object);

        // Kiểm tra đúng player bị chết
        if (player != m_player1 && player != m_player2) return;
        if (m_hasCompletedLevel) return;

        // Lấy setting từ Mod
        double fadeSpeed = Mod::get()->getSettingValue<double>("fade-speed");
        OsuFail::start(static_cast<float>(fadeSpeed));
    }

    void resetLevel()
    {
        OsuFail::reset();
        PlayLayer::resetLevel();
    }

    bool init(GJGameLevel* level, bool useReplay, bool dontRun)
    {
        OsuFail::reset();
        return PlayLayer::init(level, useReplay, dontRun);
    }

    void onQuit()
    {
        OsuFail::reset();
        PlayLayer::onQuit();
    }
};