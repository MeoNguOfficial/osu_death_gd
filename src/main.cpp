#include <Geode/Geode.hpp>

// Quan trọng: Phải include đủ các header cho những class bạn muốn hook
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>

using namespace geode::prelude;

// --- PHẦN 1: NÚT BẤM Ở MENU (Code cũ của bạn) ---
class $modify(MyMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        auto myButton = CCMenuItemSpriteExtra::create(
            CCSprite::createWithSpriteFrameName("GJ_likeBtn_001.png"),
            this,
            menu_selector(MyMenuLayer::onMyButton)
        );

        auto menu = this->getChildByID("bottom-menu");
        if (menu) {
            menu->addChild(myButton);
            myButton->setID("my-button"_spr);
            menu->updateLayout();
        }

        return true;
    }

    void onMyButton(CCObject*) {
        FLAlertLayer::create("Geode", "Hello from my custom mod!", "OK")->show();
    }
};

// --- PHẦN 2: HIỆU ỨNG OSU DEATH (Code làm méo âm thanh) ---
class $modify(MyPlayLayer, PlayLayer) {
    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        PlayLayer::destroyPlayer(player, obj);

        // Làm méo nhạc khi hẻo
        auto engine = FMODAudioEngine::sharedEngine();
        engine->m_system->setPitch(0.35f); 
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        
        // Trả nhạc về bình thường khi hồi sinh
        auto engine = FMODAudioEngine::sharedEngine();
        engine->m_system->setPitch(1.0f);
    }
};