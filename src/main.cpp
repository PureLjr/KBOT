#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <cmath>
#include <fstream>
#include <vector>

using namespace geode::prelude;

enum class BotMode { Off, Record, Replay, Auto };

struct Action {
    int frame;
    bool down;
    int button;
    bool p1;
};

namespace bot {
    inline BotMode mode = BotMode::Off;
    inline int frame = 0;
    inline size_t cursor = 0;
    inline int releaseAt = -1;
    inline std::vector<Action> macro;

    inline std::filesystem::path file() {
        return Mod::get()->getSaveDir() / "macro.txt";
    }

    inline void save() {
        std::ofstream out(file());
        for (auto& a : macro)
            out << a.frame << ' ' << a.down << ' ' << a.button << ' ' << a.p1 << '\n';
    }

    inline void load() {
        macro.clear();
        std::ifstream in(file());
        Action a{};
        while (in >> a.frame >> a.down >> a.button >> a.p1) macro.push_back(a);
    }
}

class $modify(BotLayer, GJBaseGameLayer) {
    void processCommands(float dt) {
        if (!PlayLayer::get() || bot::mode == BotMode::Off) {
            GJBaseGameLayer::processCommands(dt);
            return;
        }

        if (bot::mode == BotMode::Replay) {
            while (bot::cursor < bot::macro.size() && bot::macro[bot::cursor].frame <= bot::frame) {
                auto& a = bot::macro[bot::cursor++];
                GJBaseGameLayer::handleButton(a.down, a.button, a.p1);
            }
        } else if (bot::mode == BotMode::Auto) {
            this->autoStep();
        }

        GJBaseGameLayer::processCommands(dt);
        bot::frame++;
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        if (bot::mode == BotMode::Record && PlayLayer::get())
            bot::macro.push_back({bot::frame, down, button, isPlayer1});
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void autoStep() {
        auto p = m_player1;
        if (!p) return;

        if (bot::releaseAt >= 0 && bot::frame >= bot::releaseAt) {
            GJBaseGameLayer::handleButton(false, 1, true);
            bot::releaseAt = -1;
        }
        if (bot::releaseAt >= 0 || !p->m_isOnGround) return;

        bool cube = !p->m_isShip && !p->m_isBall && !p->m_isBird && !p->m_isDart &&
                    !p->m_isRobot && !p->m_isSpider && !p->m_isSwing;
        if (!cube) return;

        float px = p->getPositionX();
        float py = p->getPositionY();
        float lo = 25.f * p->m_playerSpeed;
        float hi = 60.f * p->m_playerSpeed;

        for (auto obj : CCArrayExt<GameObject*>(m_objects)) {
            if (obj->m_objectType != GameObjectType::Hazard) continue;
            float dx = obj->getPositionX() - px;
            if (dx < lo || dx > hi) continue;
            if (std::fabs(obj->getPositionY() - py) > 30.f) continue;

            GJBaseGameLayer::handleButton(true, 1, true);
            bot::releaseAt = bot::frame + 8;
            break;
        }
    }
};

class $modify(BotPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        auto menu = CCMenu::create();
        menu->setPosition(0, 0);
        auto win = CCDirector::get()->getWinSize();

        auto add = [&](char const* label, SEL_MenuHandler cb, float x) {
            auto spr = ButtonSprite::create(label);
            spr->setScale(0.5f);
            auto item = CCMenuItemSpriteExtra::create(spr, this, cb);
            item->setPosition(x, win.height - 20.f);
            menu->addChild(item);
        };
        add("Off", menu_selector(BotPlayLayer::onOff), 40.f);
        add("Rec", menu_selector(BotPlayLayer::onRec), 100.f);
        add("Play", menu_selector(BotPlayLayer::onPlay), 160.f);
        add("Auto", menu_selector(BotPlayLayer::onAuto), 220.f);
        add("Save", menu_selector(BotPlayLayer::onSave), 280.f);

        this->addChild(menu, 100);
        return true;
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        bot::frame = 0;
        bot::cursor = 0;
        bot::releaseAt = -1;
        if (bot::mode == BotMode::Record) bot::macro.clear();
    }

    void onOff(CCObject*) { bot::mode = BotMode::Off; }
    void onRec(CCObject*) { bot::mode = BotMode::Record; this->resetLevel(); }
    void onPlay(CCObject*) {
        if (bot::macro.empty()) bot::load();
        bot::mode = BotMode::Replay;
        this->resetLevel();
    }
    void onAuto(CCObject*) { bot::mode = BotMode::Auto; this->resetLevel(); }
    void onSave(CCObject*) { bot::save(); }
};
