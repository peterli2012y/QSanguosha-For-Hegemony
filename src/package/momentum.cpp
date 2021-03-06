/********************************************************************
    Copyright (c) 2013-2014 - QSanguosha-Hegemony Team

  This file is part of QSanguosha-Hegemony.

  This game is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 3.0 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  See the LICENSE file for more details.

  QSanguosha-Hegemony Team
*********************************************************************/

#include "momentum.h"
#include "standard-basics.h"
#include "standard-tricks.h"
#include "standard-wu-generals.h"
#include "standard-shu-generals.h"
#include "client.h"
#include "engine.h"
#include "structs.h"
#include "jsonutils.h"

class Xunxun: public PhaseChangeSkill {
public:
    Xunxun(): PhaseChangeSkill("xunxun") {
        frequency = Frequent;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *lidian, QVariant &, ServerPlayer* &) const {
        return (PhaseChangeSkill::triggerable(lidian) && lidian->getPhase() == Player::Draw) ? QStringList(objectName()) : QStringList();
    }

    virtual bool cost(TriggerEvent , Room *room, ServerPlayer *lidian, QVariant &, ServerPlayer *) const {
        if (lidian->askForSkillInvoke(objectName())){
            room->broadcastSkillInvoke(objectName());
            return true;
        }

        return false;
    }

    virtual bool onPhaseChange(ServerPlayer *lidian) const{
        Room *room = lidian->getRoom();
        room->notifySkillInvoked(lidian, objectName());

        QList<int> card_ids = room->getNCards(4);
        QList<int> original_card_ids = card_ids;
        QList<int> obtained;
        room->fillAG(card_ids, lidian);
        int id1 = room->askForAG(lidian, card_ids, false, objectName());
        card_ids.removeOne(id1);
        obtained << id1;
        room->clearAG(lidian);
        room->fillAG(original_card_ids, lidian, obtained);
        int id2 = room->askForAG(lidian, card_ids, false, objectName());
        card_ids.removeOne(id2);
        obtained << id2;
        room->clearAG(lidian);

        DummyCard dummy(obtained);
        lidian->obtainCard(&dummy, false);
        room->askForGuanxing(lidian, card_ids, Room::GuanxingDownOnly);

        return true;
    }
};

class Wangxi: public TriggerSkill {
public:
    Wangxi(): TriggerSkill("wangxi") {
        events << Damage << Damaged;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const {
        if (!TriggerSkill::triggerable(player)) return QStringList();
        DamageStruct damage = data.value<DamageStruct>();
        ServerPlayer *target = NULL;
        if (triggerEvent == Damage)
            target = damage.to;
        else
            target = damage.from;
        if (!target || !target->isAlive() || target == player || target->hasFlag("Global_DFDebut")) return QStringList();

        QStringList trigger_list;

        for (int i = 1; i <= damage.damage; i++)
            trigger_list << objectName();

        return trigger_list;
    }

    virtual bool cost(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const{
        DamageStruct damage = data.value<DamageStruct>();
        ServerPlayer *target = NULL;
        if (triggerEvent == Damage)
            target = damage.to;
        else
            target = damage.from;
        if (player->askForSkillInvoke(objectName(), QVariant::fromValue(target))){
            room->broadcastSkillInvoke(objectName());
            return true;
        }

        return false;
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const{
        DamageStruct damage = data.value<DamageStruct>();
        ServerPlayer *target = NULL;
        if (triggerEvent == Damage)
            target = damage.to;
        else
            target = damage.from;
        QList<ServerPlayer *> players;
        players << player << target;
        room->sortByActionOrder(players);

        room->drawCards(players, 1, objectName());

        return false;
    }
};

class Hengjiang: public MasochismSkill {
public:
    Hengjiang(): MasochismSkill("hengjiang") {
    }

    virtual QStringList triggerable(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer* &) const {
        if (!TriggerSkill::triggerable(player)) return QStringList();
        ServerPlayer *current = room->getCurrent();
        if (!current || current->isDead() || current->getPhase() == Player::NotActive) return QStringList();
        DamageStruct damage = data.value<DamageStruct>();
        QStringList trigger_skill;
        for (int i = 1; i <= damage.damage; i++)
            trigger_skill << objectName();
        return trigger_skill;
    }

    virtual bool cost(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const {
        ServerPlayer *current = room->getCurrent();
        if (current && player->askForSkillInvoke(objectName(), QVariant::fromValue((PlayerStar)current))){
            room->broadcastSkillInvoke(objectName());
            return true;
        }

        return false;
    }

    virtual void onDamaged(ServerPlayer *target, const DamageStruct &) const{
        Room *room = target->getRoom();
        ServerPlayer *current = room->getCurrent();
        if (!current) return;
        room->addPlayerMark(current, "@hengjiang");
        room->setPlayerMark(target, "HengjiangInvoke", 1);

        return;
    }
};

class HengjiangDraw: public TriggerSkill {
public:
    HengjiangDraw(): TriggerSkill("#hengjiang-draw") {
        events << TurnStart << CardsMoveOneTime << EventPhaseChanging;
        frequency = Compulsory;
    }

    virtual QMap<ServerPlayer *, QStringList> triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data) const{
        QMap<ServerPlayer *, QStringList> skill_list;
        if (!player) return skill_list;
        if (triggerEvent == TurnStart) {
            room->setPlayerMark(player, "@hengjiang", 0);
            foreach (ServerPlayer *p, room->getAllPlayers())
                if (p->getMark("HengjiangInvoke") > 0)
                    room->setPlayerMark(p, "HengjiangInvoke", 0);
        } else if (triggerEvent == CardsMoveOneTime) {
            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
            if (move.from && player == move.from && player->getPhase() == Player::Discard
                && (move.reason.m_reason & CardMoveReason::S_MASK_BASIC_REASON) == CardMoveReason::S_REASON_DISCARD)
                player->setFlags("HengjiangDiscarded");
        } else if (triggerEvent == EventPhaseChanging) {
            PhaseChangeStruct change = data.value<PhaseChangeStruct>();
            if (change.to != Player::NotActive) return skill_list;
            QList<ServerPlayer *> zangbas;
            foreach (ServerPlayer *p, room->getAllPlayers())
                if (p->getMark("HengjiangInvoke") > 0) {
                    room->setPlayerMark(p, "HengjiangInvoke", 0);
                    zangbas << p;
                }
            if (zangbas.isEmpty()) return skill_list;
            if (player->getMark("@hengjiang") > 0) {
                bool invoke = false;
                if (!player->hasFlag("HengjiangDiscarded")) {
                    LogMessage log;
                    log.type = "#HengjiangDraw";
                    log.from = player;
                    log.to = zangbas;
                    log.arg = "hengjiang";
                    room->sendLog(log);

                    invoke = true;
                } else
                    player->setFlags("-HengjiangDiscarded");
                room->setPlayerMark(player, "@hengjiang", 0);
                if (invoke)
                    foreach (ServerPlayer *zangba, zangbas)
                        skill_list.insert(zangba, QStringList(objectName()));
            }
        }
        return skill_list;
    }

    virtual bool cost(TriggerEvent , Room *, ServerPlayer *, QVariant &, ServerPlayer *ask_who) const {
        ask_who->drawCards(1);
        return false;
    }
};

class HengjiangMaxCards: public MaxCardsSkill {
public:
    HengjiangMaxCards(): MaxCardsSkill("#hengjiang-maxcard") {
    }

    virtual int getExtra(const Player *target) const{
        return -target->getMark("@hengjiang");
    }
};

class Qianxi: public TriggerSkill {
public:
    Qianxi(): TriggerSkill("qianxi") {
        events << EventPhaseStart << FinishJudge;
    }

    virtual bool canPreshow() const {
        return false;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const {
        if (!player) return QStringList();
        if (triggerEvent == EventPhaseStart && TriggerSkill::triggerable(player)
            && player->getPhase() == Player::Start) return QStringList(objectName());
        else if (triggerEvent == FinishJudge) {
            JudgeStar judge = data.value<JudgeStar>();
            if (judge->reason != objectName() || !player->isAlive()) return QStringList();

            QString color = judge->card->isRed() ? "red" : "black";
            player->tag[objectName()] = QVariant::fromValue(color);
            judge->pattern = color;
        }
        return QStringList();
    }

    virtual bool cost(TriggerEvent , Room *room, ServerPlayer *target, QVariant &, ServerPlayer *) const{
        return room->askForSkillInvoke(target, objectName());
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *target, QVariant &, ServerPlayer *) const{
        JudgeStruct judge;
        judge.reason = objectName();
        judge.play_animation = false;
        judge.who = target;

        room->judge(judge);
        if (!target->isAlive()) return false;
        QString color = judge.pattern;
        QList<ServerPlayer *> to_choose;
        foreach (ServerPlayer *p, room->getOtherPlayers(target)) {
            if (target->distanceTo(p) == 1)
                to_choose << p;
        }
        if (to_choose.isEmpty())
            return false;

        ServerPlayer *victim = room->askForPlayerChosen(target, to_choose, objectName());

        int index = 1;
        if (color == "black")
            index = 2;

        room->broadcastSkillInvoke(objectName(), index);

        QString pattern = QString(".|%1|.|hand$0").arg(color);
        room->setPlayerFlag(victim, "QianxiTarget");
        room->addPlayerMark(victim, QString("@qianxi_%1").arg(color));
        room->setPlayerCardLimitation(victim, "use,response", pattern, false);

        LogMessage log;
        log.type = "#Qianxi";
        log.from = victim;
        log.arg = QString("no_suit_%1").arg(color);
        room->sendLog(log);

        return false;
    }
};

class QianxiClear: public TriggerSkill {
public:
    QianxiClear(): TriggerSkill("#qianxi-clear") {
        events << EventPhaseChanging << Death;
        frequency = Compulsory;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer* &) const {
        if (player->tag["qianxi"].toString().isNull()) return QStringList();
        if (triggerEvent == EventPhaseChanging) {
            PhaseChangeStruct change = data.value<PhaseChangeStruct>();
            if (change.to != Player::NotActive)
                return QStringList();
        } else if (triggerEvent == Death) {
            DeathStruct death = data.value<DeathStruct>();
            if (death.who != player)
                return QStringList();
        }

        QString color = player->tag["qianxi"].toString();
        foreach (ServerPlayer *p, room->getOtherPlayers(player)) {
            if (p->hasFlag("QianxiTarget")) {
                room->removePlayerCardLimitation(p, "use,response", QString(".|%1|.|hand$0").arg(color));
                room->setPlayerMark(p, QString("@qianxi_%1").arg(color), 0);
            }
        }
        return QStringList();
    }
};

class Guixiu: public TriggerSkill {
public:
    Guixiu(): TriggerSkill("guixiu") {
        events << GameStart << GeneralShown << GeneralRemoved;
    }

    virtual bool canPreshow() const {
        return false;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer* &) const{
        if (triggerEvent == GameStart) {
            if (TriggerSkill::triggerable(player))
                room->setPlayerMark(player, objectName(), 1); // for guixiu2
        } else if (triggerEvent == GeneralShown) {
            if (TriggerSkill::triggerable(player))
                return (data.toBool() == player->inHeadSkills(objectName())) ? QStringList(objectName()) : QStringList();
        } else if (data.toString() == "mifuren" && player->getMark(objectName()) > 0)
            if (player->isWounded())
                return QStringList(objectName());

        return QStringList();
    }

    virtual bool cost(TriggerEvent, Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const{
        if (room->askForSkillInvoke(player, objectName())) {
            room->broadcastSkillInvoke(objectName());
            return true;
        }
        return false;
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const{
        if (triggerEvent == GeneralShown)
            player->drawCards(2, objectName());
        else if (triggerEvent == GeneralRemoved) {
            room->setPlayerMark(player, objectName(), 0);
            LogMessage log;
            log.type = "#InvokeSkill";
            log.from = player;
            log.arg = "guixiu";
            room->sendLog(log);

            RecoverStruct recover;
            recover.who = player;
            room->recover(player, recover);
        }
        return false;
    }
};

CunsiCard::CunsiCard() {
}

bool CunsiCard::targetFilter(const QList<const Player *> &targets, const Player *, const Player *) const{
    return targets.isEmpty();
}

void CunsiCard::onEffect(const CardEffectStruct &effect) const{
    Room *room = effect.from->getRoom();
    room->doLightbox("$CunsiAnimate", 3000);
    effect.from->removeGeneral(effect.from->inHeadSkills("cunsi"));
    room->acquireSkill(effect.to, "yongjue");
    room->setPlayerMark(effect.to, "@yongjue", 1);
    if (effect.to != effect.from)
        effect.to->drawCards(2);
}

class CunsiVS: public ZeroCardViewAsSkill {
public:
    CunsiVS(): ZeroCardViewAsSkill("cunsi") {
    }

    virtual const Card *viewAs() const{
        CunsiCard *card = new CunsiCard;
        card->setShowSkill(objectName());
        return card;
    }
};

class Cunsi: public TriggerSkill {
public:
    Cunsi(): TriggerSkill("cunsi") {
        events << GameStart << EventAcquireSkill;
        view_as_skill = new CunsiVS;
        frequency = Limited;
    }

    virtual QStringList triggerable(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer* &) const{
        if (!player || !player->isAlive() || !player->ownSkill(this)) return QStringList();
        room->getThread()->addTriggerSkill(Sanguosha->getTriggerSkill("yongjue"));
        return QStringList();
    }
};

class Yongjue: public TriggerSkill {
public:
    Yongjue(): TriggerSkill("yongjue") {
        events << CardUsed << BeforeCardsMove;
    }

    virtual bool canPreshow() const {
        return false;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer* &ask_who) const{
        if (player == NULL || !player->isAlive()) return QStringList();
        QList<ServerPlayer *> owners = room->findPlayersBySkillName(objectName());
        if (triggerEvent == CardUsed){
            CardUseStruct use = data.value<CardUseStruct>();
            if (use.from->getPhase() == Player::Play && use.from->getMark(objectName()) == 0){
                if (!use.card->isKindOf("SkillCard"))
                    use.from->addMark(objectName());
                if (use.card->isKindOf("Slash")) {
                    use.from->tag.remove("yongjue_card");
                    QList<int> ids;
                    if (!use.card->isVirtualCard())
                        ids << use.card->getEffectiveId();
                    else if (use.card->subcardsLength() > 0)
                        ids = use.card->getSubcards();
                    if (!ids.isEmpty()){
                        QVariantList card_list;
                        foreach (int id, ids){
                            card_list << id;
                        }
                        use.from->tag["yongjue_card"] = card_list;
                    }
                }
            }
        }
        else if (triggerEvent == BeforeCardsMove){
            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
            if (move.from != NULL && move.from->tag.contains("yongjue_card") && player == move.from
                    && ((move.reason.m_reason & CardMoveReason::S_MASK_BASIC_REASON) == CardMoveReason::S_REASON_USE)
                    && move.from_places.contains(Player::PlaceTable) && move.to_place == Player::DiscardPile){
                QVariantList card_list = player->tag["yongjue_card"].toList();
                player->tag.remove("yongjue_card");
                bool invoke = true;
                foreach (QVariant id_v, card_list){
                    if (!(move.card_ids.contains(id_v.toInt()) && room->getCardPlace(id_v.toInt()) == Player::PlaceTable)){
                        invoke = false;
                        break;
                    }
                }
                if (invoke){
                    ask_who = player;
                    foreach (ServerPlayer *p, owners){
                        if (p->isFriendWith(player))
                            return QStringList(objectName());
                    }
                }
            }
        }
        return QStringList();
    }

    virtual bool cost(TriggerEvent, Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const{
        QList<ServerPlayer *> owners = room->findPlayersBySkillName(objectName());
        ServerPlayer *owner = NULL;
        foreach(ServerPlayer *p, owners){
            if (player->isFriendWith(p)) {
                owner = p;
                break;
            }
        }

        if (owner != NULL && room->askForSkillInvoke(player, objectName())) {
            LogMessage log;
            log.type = "#InvokeOthersSkill";
            log.from = player;
            log.to << owner;
            log.arg = objectName();
            room->sendLog(log);
            room->broadcastSkillInvoke(objectName());
            return true;
        }
        return false;
    }

    virtual bool effect(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer *) const{
        CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
        DummyCard dummy(move.card_ids);
        move.card_ids.clear();
        data = QVariant::fromValue(move);

        player->obtainCard(&dummy);

        return false;
    }
};

class YongjueStart: public TriggerSkill {
public:
    YongjueStart(): TriggerSkill("yongjue-start") {
        events << EventPhaseStart;
        global = true;
        frequency = Compulsory;
    }

    virtual int getPriority() const{
        return 10;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *target, QVariant &, ServerPlayer* &) const {
        if (target == NULL || target->isDead()) return QStringList();
        if (target->getPhase() == Player::Play)
            target->setMark("yongjue", 0);
        return QStringList();
    }
};

class Jiang: public TriggerSkill {
public:
    Jiang(): TriggerSkill("jiang") {
        events << TargetConfirmed << TargetChosen;
        frequency = Frequent;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *sunce, QVariant &data, ServerPlayer* &) const {
        if (!TriggerSkill::triggerable(sunce)) return QStringList();
        CardUseStruct use = data.value<CardUseStruct>();
        bool invoke = false;
        if (triggerEvent == TargetChosen)
            invoke = (sunce == use.from);
        else
            invoke = (use.to.contains(sunce));

        if (invoke) {
            if (use.card->isKindOf("Duel") || (use.card->isKindOf("Slash") && use.card->isRed()))
                return QStringList(objectName());
        }
        return QStringList();
    }

    virtual bool cost(TriggerEvent, Room *room, ServerPlayer *sunce, QVariant &data, ServerPlayer *) const {
        if (sunce->askForSkillInvoke(objectName())){
            CardUseStruct use = data.value<CardUseStruct>();

            int index = 1;
            if (use.from != sunce)
                index = 2;
            room->broadcastSkillInvoke(objectName(), index);
            return true;
        }

        return false;
    }

    virtual bool effect(TriggerEvent, Room *, ServerPlayer *sunce, QVariant &, ServerPlayer *) const {
        sunce->drawCards(1);
        return false;
    }
};

class Yingyang: public TriggerSkill {
public:
    Yingyang(): TriggerSkill("yingyang") {
        events << PindianVerifying;
    }

    virtual QMap<ServerPlayer *, QStringList> triggerable(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data) const {
        QMap<ServerPlayer *, QStringList> skill_list;
        if (player == NULL) return skill_list;
        QList<ServerPlayer *> sunces = room->findPlayersBySkillName(objectName());
        PindianStar pindian = data.value<PindianStar>();
        foreach (ServerPlayer *sunce, sunces)
            if (pindian->from == sunce || pindian->to == sunce)
                skill_list.insert(sunce, QStringList(objectName()));

        return skill_list;
    }

    virtual bool cost(TriggerEvent, Room *, ServerPlayer *, QVariant &data, ServerPlayer *ask_who) const{
        return ask_who->askForSkillInvoke(objectName(), data);
    }

    virtual bool effect(TriggerEvent, Room* room, ServerPlayer *, QVariant &data, ServerPlayer *ask_who) const{
        ServerPlayer *sunce = ask_who;
        PindianStar pindian = data.value<PindianStar>();
        bool isFrom = pindian->from == sunce;

        QString choice = room->askForChoice(sunce, objectName(), "jia3+jian3", data);

        int index = 2;
        if (choice == "jia3")
            index = 1;

        room->broadcastSkillInvoke(choice, index);

        LogMessage log;
        log.type = "$Yingyang";
        log.from = sunce;

        if (isFrom) {
            pindian->from_number = choice == "jia3" ? qMin(pindian->from_number + 3 , 13) : qMax(pindian->from_number - 3 , 1);

            log.arg = QString::number(pindian->from_number);
        } else {
            pindian->to_number = choice == "jia3" ? qMin(pindian->to_number + 3 , 13) : qMax(pindian->to_number - 3 , 1);

            log.arg = QString::number(pindian->to_number);
        }

        room->sendLog(log);

        return false;
    }
};

class Yinghun_Sunce: public Yinghun{
public:
    Yinghun_Sunce(): Yinghun(){
        setObjectName("yinghun_sunce");
    }
};

class Yingzi_Sunce: public Yingzi{
public:
    Yingzi_Sunce(): Yingzi(){
        setObjectName("yingzi_sunce");
    }

    virtual bool canPreshow() const {
        return false;
    }
};

class Hunshang: public TriggerSkill {
public:
    Hunshang(): TriggerSkill("hunshang") {
        frequency = Compulsory;
        events << GameStart << EventPhaseStart;
        relate_to_place = "deputy";
    }

    virtual bool canPreshow() const {
        return false;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &, ServerPlayer* &) const {
        if (!TriggerSkill::triggerable(player)) return QStringList();
        if (triggerEvent == GameStart) {
            const Skill *yinghun = Sanguosha->getSkill("yinghun_sunce");
            if (yinghun != NULL && yinghun->inherits("TriggerSkill")){
                const TriggerSkill *yinghun_trigger = qobject_cast<const TriggerSkill *>(yinghun);
                room->getThread()->addTriggerSkill(yinghun_trigger);
            }

            return QStringList();
        }
        return (player->getPhase() == Player::Start && player->getHp() == 1) ? QStringList(objectName()) : QStringList();
    }

    virtual bool cost(TriggerEvent , Room* room, ServerPlayer *player, QVariant &, ServerPlayer *) const {
        bool invoke = player->hasShownSkill(this) ? true : room->askForSkillInvoke(player, objectName());
        if (invoke){
            room->broadcastSkillInvoke(objectName());
            return true;
        }
        return false;
    }

    virtual bool effect(TriggerEvent , Room* room, ServerPlayer *target, QVariant &, ServerPlayer *) const{
        QStringList skills;
        skills << "yinghun_sunce!" << "yingzi_sunce!";
        room->handleAcquireDetachSkills(target, skills);
        target->setMark("hunshang", 1);
        return false;
    }
};

class HunshangRemove: public TriggerSkill {
public:
    HunshangRemove(): TriggerSkill("#hunshang") {
        frequency = Compulsory;
        events << EventPhaseStart;
    }

    virtual bool canPreshow() const {
        return false;
    }

    virtual bool triggerable(const ServerPlayer *player) const {
        if (player != NULL && player->getPhase() == Player::NotActive && player->getMark("hunshang") > 0)
            return true;
        return false;
    }

    virtual bool effect(TriggerEvent , Room* room, ServerPlayer *player, QVariant &, ServerPlayer *) const{
        player->setMark("hunshang", 0);
        room->handleAcquireDetachSkills(player, "-yinghun_sunce|-yingzi_sunce", true);

        return false;
    }
};

DuanxieCard::DuanxieCard() {
}

bool DuanxieCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const{
    return targets.isEmpty() && to_select != Self;
}

void DuanxieCard::onEffect(const CardEffectStruct &effect) const{
    Room *room = effect.from->getRoom();
    room->setPlayerProperty(effect.to, "chained", true);
    if (!effect.from->isChained())
        room->setPlayerProperty(effect.from, "chained", true);
}

class Duanxie : public ZeroCardViewAsSkill {
public:
    Duanxie(): ZeroCardViewAsSkill("duanxie") {
    }

    virtual bool isEnabledAtPlay(const Player *player) const{
        return !player->hasUsed("DuanxieCard");
    }

    virtual const Card *viewAs() const{
        Card *card = new DuanxieCard;
        card->setShowSkill(objectName());
        return card;
    }
};

class Fenming: public PhaseChangeSkill {
public:
    Fenming(): PhaseChangeSkill("fenming") {
    }

    virtual QStringList triggerable(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer* &) const {
        if (PhaseChangeSkill::triggerable(player) && player->getPhase() == Player::Finish && player->isChained())
            foreach (ServerPlayer *p, room->getAllPlayers())
                if (p->isChained() && player->canDiscard(p, "he"))
                    return QStringList(objectName());

        return QStringList();
    }

    virtual bool cost(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const {
        if (player->askForSkillInvoke(objectName())){
            room->broadcastSkillInvoke(objectName());
            return true;
        }

        return false;
    }

    virtual bool onPhaseChange(ServerPlayer *player) const{
        Room *room = player->getRoom();
        foreach (ServerPlayer *p, room->getAllPlayers())
            if (p->isChained() && player->canDiscard(p, "he")) {
                if (player != p){
                    int card_id = room->askForCardChosen(player, p, "he", objectName(), false, Card::MethodDiscard);
                    room->throwCard(card_id, p, player);
                }
                else
                    room->askForDiscard(player, objectName(), 1, 1, false, true);
            }
        return false;
    }
};

class Hengzheng: public PhaseChangeSkill {
public:
    Hengzheng(): PhaseChangeSkill("hengzheng") {
    }

    virtual QStringList triggerable(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer* &) const {
        if (PhaseChangeSkill::triggerable(player) && player->getPhase() == Player::Draw && (player->isKongcheng() || player->getHp() == 1))
            foreach (ServerPlayer *p, room->getOtherPlayers(player))
                if (!p->isAllNude())
                    return QStringList(objectName());

        return QStringList();
    }

    virtual bool cost(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const {
        if (player->askForSkillInvoke(objectName())){
            room->broadcastSkillInvoke(objectName());
            room->doLightbox("$HengzhengAnimate", 4000);
            return true;
        }

        return false;
    }

    virtual bool onPhaseChange(ServerPlayer *player) const{
        Room *room = player->getRoom();
        foreach (ServerPlayer *p, room->getOtherPlayers(player))
            if (!p->isAllNude()) {
                int card_id = room->askForCardChosen(player, p, "hej", objectName());
                room->obtainCard(player, card_id, false);
            }
        return true;
    }
};

class Baoling: public TriggerSkill {
public:
    Baoling(): TriggerSkill("baoling") {
        events << EventPhaseEnd;
        relate_to_place = "head";
    }

    virtual bool canPreshow() const {
        return false;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &, ServerPlayer* &) const {
        if (TriggerSkill::triggerable(player) && player->getPhase() == Player::Play && player->hasShownSkill(this))
            return (player->getActualGeneral2Name().contains("sujiang")) ? QStringList() : QStringList(objectName());

        return QStringList();
    }

    virtual bool cost(TriggerEvent , Room *room, ServerPlayer *, QVariant &, ServerPlayer *) const {
        room->broadcastSkillInvoke(objectName());
        room->doLightbox("$BaolingAnimate", 3000);
        return true;
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const {
        player->removeGeneral(false);
        room->setPlayerProperty(player, "maxhp", player->getMaxHp() + 3);

        LogMessage log;
        log.type = "#GainMaxHp";
        log.from = player;
        log.arg = QString::number(3);
        room->sendLog(log);

        RecoverStruct recover;
        recover.recover = 3;
        recover.who = player;
        room->recover(player, recover);

        room->handleAcquireDetachSkills(player, "benghuai");
        return false;
    }
};

class Benghuai: public PhaseChangeSkill {
public:
    Benghuai(): PhaseChangeSkill("benghuai") {
        frequency = Compulsory;
    }

    virtual bool canPreshow() const {
        return false;
    }

    virtual QStringList triggerable(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer* &) const {
        if (!PhaseChangeSkill::triggerable(player)) return QStringList();
        if (player->getPhase() == Player::Finish) {
            QList<ServerPlayer *> players = room->getOtherPlayers(player);
            foreach (ServerPlayer *p, players)
                if (player->getHp() > p->getHp())
                    return QStringList(objectName());
        }

        return QStringList();
    }

    virtual bool onPhaseChange(ServerPlayer *dongzhuo) const{
        Room *room = dongzhuo->getRoom();
        LogMessage log;
        log.from = dongzhuo;
        log.arg = objectName();
        log.type = "#TriggerSkill";
        room->sendLog(log);
        room->notifySkillInvoked(dongzhuo, objectName());

        QString result = room->askForChoice(dongzhuo, "benghuai", "hp+maxhp");
        int index = (result == "hp") ? 2 : 1;
        room->broadcastSkillInvoke(objectName(), index);
        if (result == "hp")
            room->loseHp(dongzhuo);
        else
            room->loseMaxHp(dongzhuo);

        return false;
    }
};

class Chuanxin: public TriggerSkill {
public:
    Chuanxin(): TriggerSkill("chuanxin") {
        events << DamageCaused;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const {
        if (!TriggerSkill::triggerable(player)) return QStringList();
        DamageStruct damage = data.value<DamageStruct>();
        if (!damage.to || !damage.to->hasShownOneGeneral()) return QStringList();
        if (!damage.card || !(damage.card->isKindOf("Slash") || damage.card->isKindOf("Duel"))) return QStringList();
        if (!player->hasShownOneGeneral() || player->getPhase() != Player::Play) return QStringList();
        if (player->isFriendWith(damage.to)) return QStringList();
        if (damage.transfer || damage.chain) return QStringList();
        if (damage.to->getActualGeneral2Name().contains("sujiang")) return QStringList();
        return QStringList(objectName());
    }

    virtual bool cost(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer *) const {
        return player->askForSkillInvoke(objectName(), data);
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *, QVariant &data, ServerPlayer *) const {
        DamageStruct damage = data.value<DamageStruct>();
        QStringList choices;
        if (damage.to->hasEquip())
            choices << "discard";
        choices << "remove";
        QString choice = room->askForChoice(damage.to, objectName(), choices.join("+"));
        if (choice == "discard") {
            room->broadcastSkillInvoke(objectName(), 1);
            damage.to->throwAllEquips();
            room->loseHp(damage.to);
        }
        else {
            room->broadcastSkillInvoke(objectName(), 2);
            damage.to->removeGeneral(false);
        }

        return true;
    }
};

FengshiSummon::FengshiSummon()
    : ArraySummonCard("fengshi")
{
    m_skillName = "fengshi";
}

class Fengshi: public BattleArraySkill {
public:
    Fengshi(): BattleArraySkill("fengshi", HegemonyMode::Siege) {
        events << TargetChosen;
    }

    virtual bool canPreshow() const{
        return false;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &ask_who) const{
        if (!TriggerSkill::triggerable(player)) return QStringList();
        if (!player->hasShownSkill(this) || player->aliveCount() < 4) return QStringList();
        CardUseStruct use = data.value<CardUseStruct>();
        if (use.card->isKindOf("Slash"))
            foreach (ServerPlayer *to, use.to)
                if (use.from->inSiegeRelation(player, to))
                    if (to->canDiscard(to, "e")) {
                        ask_who = player;
                        return QStringList(objectName());
                    }

        return QStringList();
    }

    virtual bool effect(TriggerEvent , Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const {
        CardUseStruct use = data.value<CardUseStruct>();
        if (use.card->isKindOf("Slash"))
            foreach (ServerPlayer *to, use.to)
                if (use.from->inSiegeRelation(player, to))
                    if (to->canDiscard(to, "e")) {
                        int card_id = room->askForCardChosen(to, to, "e", objectName(), true, Card::MethodDiscard);
                        room->throwCard(card_id, to);
                    }
        return false;
    }
};

class Wuxin: public PhaseChangeSkill {
public:
    Wuxin(): PhaseChangeSkill("wuxin") {
    }

    virtual bool canPreshow() const {
        return false;
    }

    virtual QStringList triggerable(TriggerEvent , Room *, ServerPlayer *player, QVariant &, ServerPlayer* &) const {
        if (!PhaseChangeSkill::triggerable(player)) return QStringList();
        if (player->getPhase() == Player::Draw) return QStringList(objectName());

        return QStringList();
    }

    virtual bool cost(TriggerEvent , Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const {
        if (player->askForSkillInvoke(objectName())){
            room->broadcastSkillInvoke(objectName());
            return true;
        }

        return false;
    }

    virtual bool onPhaseChange(ServerPlayer *player) const{
        Room *room = player->getRoom();
        int num = player->getPlayerNumWithSameKingdom();

        QList<int> guanxing = room->getNCards(num);

        LogMessage log;
        log.type = "$ViewDrawPile";
        log.from = player;
        log.card_str = IntList2StringList(guanxing).join("+");
        room->doNotify(player, QSanProtocol::S_COMMAND_LOG_SKILL, log.toJsonValue());

        room->askForGuanxing(player, guanxing, Room::GuanxingUpOnly);

        return false;
    }
};

HongfaCard::HongfaCard() {
    target_fixed = true;
    m_skillName = "hongfa_slash";
    mute = true;
}

void HongfaCard::onUse(Room *room, const CardUseStruct &card_use) const{
    ServerPlayer *qunxiong = card_use.from;
    const Player *zhangjiao = qunxiong->getLord();
    if (!zhangjiao || !zhangjiao->isFriendWith(qunxiong)) return;
    QList<int> tianbings;
    QList<int> total = zhangjiao->getPile("heavenly_army");
    foreach (int id, total) {
        Slash *slash = new Slash(Card::SuitToBeDecided, -1);
        slash->addSubcard(id);
        if (!Slash::IsAvailable(qunxiong, slash))
            continue;
        foreach (ServerPlayer *p, room->getAlivePlayers()) {
            if (!slash->targetFilter(QList<const Player *>(), p, qunxiong))
                continue;
            if (qunxiong->isProhibited(p, slash))
                continue;
            tianbings << id;
            break;
        }
        delete slash;
        slash = NULL;
    }

    if (tianbings.isEmpty())
        return;

    QList<int> disabled;
    foreach (int id, total) {
        if (!tianbings.contains(id))
            disabled << id;
    }

    int card_id;
    if (tianbings.length() == 1)
        card_id = tianbings.first();
    else {
        room->fillAG(total, qunxiong, disabled);
        card_id = room->askForAG(qunxiong, tianbings, false, "hongfa");
        room->clearAG(qunxiong);

        if (card_id == -1)
            return;
    }

    Slash *slash = new Slash(Card::SuitToBeDecided, -1);
    slash->setSkillName("hongfa");
    slash->addSubcard(card_id);

    QList<ServerPlayer *> targets;
    foreach (ServerPlayer *p, room->getAlivePlayers()) {
        if (!slash->targetFilter(QList<const Player *>(), p, qunxiong))
            continue;
        if (qunxiong->isProhibited(p, slash))
            continue;

        targets << p;
    }
    if (targets.isEmpty())
        return;

    room->setPlayerProperty(qunxiong, "hongfa_slash", slash->toString());

    CardUseStruct use;
    use.card = slash;
    use.from = qunxiong;

    if (room->askForUseCard(qunxiong, "@@hongfa_slash!", "@hongfa-target")) {
        foreach (ServerPlayer *p, room->getAlivePlayers()) {
            if (p->hasFlag("HongfaTarget")) {
                room->setPlayerFlag(p, "-HongfaTarget");
                use.to << p;
            }
        }
    } else {
        use.to << targets.at(qrand() % targets.length());
    }
    room->setPlayerProperty(qunxiong, "hongfa_slash", QString());

    room->useCard(use);
}

HongfaSlashCard::HongfaSlashCard() {
    m_skillName = "hongfa_slash";
    mute = true;
}

bool HongfaSlashCard::targetFilter(const QList<const Player *> &targets, const Player *to_select, const Player *Self) const{
    const Card *card = Card::Parse(Self->property("hongfa_slash").toString());
    if (card == NULL)
        return false;
    else {
        const Slash *slash = qobject_cast<const Slash *>(card);
        return !Self->isProhibited(to_select, slash, targets) && slash->targetFilter(targets, to_select, Self);
    }
}

void HongfaSlashCard::onUse(Room *room, const CardUseStruct &card_use) const{
    foreach (ServerPlayer *to, card_use.to)
        room->setPlayerFlag(to, "HongfaTarget");
}

class HongfaSlash: public ZeroCardViewAsSkill {
public:
    HongfaSlash(): ZeroCardViewAsSkill("hongfa_slash") {
        attached_lord_skill = true;
    }

    virtual bool isEnabledAtPlay(const Player *player) const{
        if (!player->hasShownOneGeneral())
            return false;
        const Player *zhangjiao = player->getLord();
        if (!zhangjiao || zhangjiao->getPile("heavenly_army").isEmpty() || !zhangjiao->isFriendWith(player))
            return false;
        foreach (int id, zhangjiao->getPile("heavenly_army")) {
            Slash *slash = new Slash(Card::SuitToBeDecided, -1);
            slash->setSkillName("hongfa");
            slash->addSubcard(id);
            slash->deleteLater();
            if (!Slash::IsAvailable(player, slash))
                continue;
            foreach (const Player *p, player->getAliveSiblings()) {
                if (!slash->targetFilter(QList<const Player *>(), p, player))
                    continue;
                if (player->isProhibited(p, slash))
                    continue;
                return true;
            }
        }
        return false;
    }

    virtual bool isEnabledAtResponse(const Player *player, const QString &pattern) const{
        if (!player->hasShownOneGeneral())
            return false;
        const Player *zhangjiao = player->getLord();
        if (!zhangjiao || zhangjiao->getPile("heavenly_army").isEmpty() || !zhangjiao->isFriendWith(player))
            return false;
        if (pattern == "slash" && Sanguosha->currentRoomState()->getCurrentCardUseReason() == CardUseStruct::CARD_USE_REASON_RESPONSE_USE)
            return true;
        return pattern == "@@hongfa_slash!";
    }

    virtual const Card *viewAs() const{
        QString pattern = Sanguosha->currentRoomState()->getCurrentCardUsePattern();
        if (pattern == "@@hongfa_slash!")
            return new HongfaSlashCard;
        else
            return new HongfaCard;
    }
};

class HongfaSlashResp: public TriggerSkill{
public:
    HongfaSlashResp(): TriggerSkill("hongfa_slash_resp"){
        events << CardAsked;
        global = true;
    }

    virtual QStringList triggerable(TriggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer * &) const{
        if (player != NULL && player->isAlive() && player->hasShownOneGeneral()){
            ServerPlayer *zhangjiao = room->getLord(player->getKingdom());
            if (zhangjiao == NULL || !zhangjiao->hasSkill("hongfa") || zhangjiao->getPile("heavenly_army").isEmpty())
                return QStringList();

            QStringList ask = data.toStringList();
            if (ask[0] == "slash"){
                QList<int> can_use = zhangjiao->getPile("heavenly_army");
                foreach (int id, zhangjiao->getPile("heavenly_army")){
                    if (player->isCardLimited(Sanguosha->getCard(id), Card::MethodResponse, false))
                        can_use.removeOne(id);
                }

                if (can_use.isEmpty())
                    return QStringList();

                return QStringList(objectName());
            }
        }
        return QStringList();
    }

    virtual bool cost(TriggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer *) const{
        return player->askForSkillInvoke(objectName(), data);
    }

    virtual bool effect(TriggerEvent, Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const{
        ServerPlayer *zhangjiao = room->getLord(player->getKingdom());
        QList<int> can_use = zhangjiao->getPile("heavenly_army"), can_not_use;
        foreach (int id, zhangjiao->getPile("heavenly_army")){
            if (player->isCardLimited(Sanguosha->getCard(id), Card::MethodResponse, false)){
                can_use.removeOne(id);
                can_not_use << id;
            }
        }

        if (!can_use.isEmpty()){
            room->fillAG(zhangjiao->getPile("heavenly_army"), player, can_not_use);
            int id = room->askForAG(player, can_use, false, "hongfa");
            room->clearAG(player);

            Slash *slash = new Slash(Card::SuitToBeDecided, -1);
            slash->setSkillName("hongfa");
            slash->addSubcard(id);

            room->provide(slash);
            return true;
        }
        return false;
    }
};

class Hongfa: public TriggerSkill {
public:
    Hongfa(): TriggerSkill("hongfa$") {
        events << EventPhaseStart << PreHpLost << GeneralShown << GeneralHidden << GeneralRemoved << Death;
        frequency = Compulsory;
    }

    virtual bool canPreshow() const {
        return false;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer* &) const {
        if (triggerEvent == EventPhaseStart) {
            if (!player || !player->isAlive() || !player->hasLordSkill(objectName())) return QStringList();
            if (player->getPhase() != Player::Start) return QStringList();
            if (!player->getPile("heavenly_army").isEmpty()) return QStringList();
            return QStringList(objectName());
        } else if (triggerEvent == PreHpLost) {
            if (!player || !player->isAlive() || !player->hasLordSkill(objectName())) return QStringList();
            if (player->getPile("heavenly_army").isEmpty()) return QStringList();
            return QStringList(objectName());
        } else {
            if (player == NULL)
                return QStringList();
            if (triggerEvent == GeneralShown){
                if (player && player->isAlive() && player->hasLordSkill(objectName())){
                    foreach (ServerPlayer *p, room->getAlivePlayers()){
                        if (p->willBeFriendWith(player) || p->getKingdom() == player->getKingdom()){
                            room->attachSkillToPlayer(p, "hongfa_slash");
                        }
                    }
                }
                else {
                    ServerPlayer *lord = room->getLord(player->getKingdom());
                    if (lord && lord->isAlive() && lord->hasLordSkill(objectName())){
                        room->attachSkillToPlayer(player, "hongfa_slash");
                    }
                }
            }
            else if (player && player->isAlive() && player->hasLordSkill(objectName())){
                if (triggerEvent == Death){
                    DeathStruct death = data.value<DeathStruct>();
                    if (death.who != player)
                        return QStringList();
                }
                foreach (ServerPlayer *p, room->getAlivePlayers()){
                    room->detachSkillFromPlayer(p, "hongfa_slash");
                }
            }
            return QStringList();
        }
        return QStringList();
    }

    virtual bool cost(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const {
        if (triggerEvent == EventPhaseStart) return true;
        if (triggerEvent == PreHpLost) {
            if (player->askForSkillInvoke(objectName())){
                room->broadcastSkillInvoke(objectName(), 2);
                return true;
            }
        }
        return false;
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &, ServerPlayer *) const {
        if (triggerEvent == EventPhaseStart) {
            int num = player->getPlayerNumWithSameKingdom();
            QList<int> tianbing = room->getNCards(num);
            player->addToPile("heavenly_army", tianbing);
            return false;
        } else {
            room->fillAG(player->getPile("heavenly_army"), player);
            int card_id = room->askForAG(player, player->getPile("heavenly_army"), false, "hongfa");
            room->clearAG(player);

            if (card_id != -1) {
                CardMoveReason reason(CardMoveReason::S_REASON_REMOVE_FROM_PILE, QString(), objectName(), QString());
                room->throwCard(Sanguosha->getCard(card_id), reason, NULL);
                return true;
            }
        }

        return false;
    }

    virtual int getEffectIndex(const ServerPlayer *, const Card *card) const{
        if (card->isKindOf("Slash"))
            return 1;
        return 0;
    }
};

WendaoCard::WendaoCard(){
    target_fixed = true;
}

void WendaoCard::use(Room *room, ServerPlayer *source, QList<ServerPlayer *> &) const{
    const Card *tpys = NULL;
    foreach(ServerPlayer *p, room->getAlivePlayers()){
        foreach(const Card *card, p->getEquips()){
            if (Sanguosha->getCard(card->getEffectiveId())->isKindOf("PeaceSpell")){
                tpys = Sanguosha->getCard(card->getEffectiveId());
                break;
            }
        }
        if (tpys != NULL)
            break;
    }
    if (tpys == NULL)
        foreach(int id, room->getDiscardPile()){
            if (Sanguosha->getCard(id)->isKindOf("PeaceSpell")){
                tpys = Sanguosha->getCard(id);
                break;
            }
    }

    if (tpys == NULL)
        return;

    source->obtainCard(tpys, true);
}

class Wendao: public OneCardViewAsSkill {
public:
    Wendao(): OneCardViewAsSkill("wendao") {
        filter_pattern = ".|red!";
    }
    virtual bool isEnabledAtPlay(const Player *player) const{
        return !player->hasUsed("WendaoCard") && player->canDiscard(player, "he");
    }

    virtual const Card *viewAs(const Card *originalCard) const{
        WendaoCard *card = new WendaoCard;
        card->addSubcard(originalCard);
        card->setShowSkill(objectName());
        return card;
    }
};

MomentumPackage::MomentumPackage()
    : Package("momentum")
{
    General *lidian = new General(this, "lidian", "wei", 3); // WEI 017
    lidian->addCompanion("yuejin");
    lidian->addSkill(new Xunxun);
    lidian->addSkill(new Wangxi);

    General *zangba = new General(this, "zangba", "wei", 4); // WEI 023
    zangba->addCompanion("zhangliao");
    zangba->addSkill(new Hengjiang);
    zangba->addSkill(new HengjiangDraw);
    zangba->addSkill(new HengjiangMaxCards);
    related_skills.insertMulti("hengjiang", "#hengjiang-draw");
    related_skills.insertMulti("hengjiang", "#hengjiang-maxcard");

    General *madai = new General(this, "madai", "shu", 4); // SHU 019
    madai->addCompanion("machao");
    madai->addSkill(new Mashu("madai"));
    madai->addSkill(new Qianxi);
    madai->addSkill(new QianxiClear);
    related_skills.insertMulti("qianxi", "#qianxi-clear");

    General *mifuren = new General(this, "mifuren", "shu", 3, false); // SHU 021
    mifuren->addSkill(new Guixiu);
    mifuren->addSkill(new Cunsi);
    mifuren->addRelateSkill("yongjue");

    General *sunce = new General(this, "sunce", "wu", 4); // WU 010
    sunce->addCompanion("zhouyu");
    sunce->addCompanion("taishici");
    sunce->addCompanion("daqiao");
    sunce->addSkill(new Jiang);
    sunce->addSkill(new Yingyang);
    sunce->addSkill(new Hunshang);
    sunce->addSkill(new HunshangRemove);
    related_skills.insertMulti("hunshang", "#hunshang");
    sunce->setDeputyMaxHpAdjustedValue(-1);
    sunce->addRelateSkill("yinghun_sunce");
    sunce->addRelateSkill("yingzi_sunce");

    General *chenwudongxi = new General(this, "chenwudongxi", "wu", 4); // WU 023
    chenwudongxi->addSkill(new Duanxie);
    chenwudongxi->addSkill(new Fenming);

    General *dongzhuo = new General(this, "dongzhuo", "qun", 4); // QUN 006
    dongzhuo->addSkill(new Hengzheng);
    dongzhuo->addSkill(new Baoling);
    dongzhuo->addRelateSkill("benghuai");

    General *zhangren = new General(this, "zhangren", "qun", 4); // QUN 024
    zhangren->addSkill(new Chuanxin);
    zhangren->addSkill(new Fengshi);

    skills << new Yongjue << new YongjueStart << new Benghuai << new HongfaSlash << new HongfaSlashResp << new Yinghun_Sunce << new Yingzi_Sunce;

    addMetaObject<CunsiCard>();
    addMetaObject<DuanxieCard>();
    addMetaObject<FengshiSummon>();
    addMetaObject<HongfaCard>();
    addMetaObject<HongfaSlashCard>();
    addMetaObject<WendaoCard>();

    General *lord_zhangjiao = new General(this, "lord_zhangjiao$", "qun");
    lord_zhangjiao->addSkill(new Wuxin);
    lord_zhangjiao->addSkill(new Hongfa);
    lord_zhangjiao->addSkill(new Wendao);
}

ADD_PACKAGE(Momentum)

PeaceSpell::PeaceSpell(Suit suit, int number)
    : Armor(suit, number)
{
    setObjectName("PeaceSpell");
}

void PeaceSpell::onUninstall(ServerPlayer *player) const{
    if (player->isAlive() && player->hasArmorEffect(objectName()))
        player->setFlags("peacespell_throwing");

    Armor::onUninstall(player);
}

class PeaceSpellSkill: public ArmorSkill{
public:
    PeaceSpellSkill(): ArmorSkill("PeaceSpell") {
        events << DamageInflicted << CardsMoveOneTime;
        frequency = Compulsory;
    }

    virtual QStringList triggerable(TriggerEvent triggerEvent, Room *, ServerPlayer *player, QVariant &data, ServerPlayer* &) const{
        if (triggerEvent == DamageInflicted) {
            DamageStruct damage = data.value<DamageStruct>();
            if (ArmorSkill::triggerable(player) && damage.nature != DamageStruct::Normal)
                return QStringList(objectName());
        } else if (player->hasFlag("peacespell_throwing")) {
            CardsMoveOneTimeStruct move = data.value<CardsMoveOneTimeStruct>();
            if (move.from != player || !move.from_places.contains(Player::PlaceEquip))
                return QStringList();
            for (int i = 0; i < move.card_ids.size(); i++) {
                if (move.from_places[i] != Player::PlaceEquip) continue;
                const Card *card = Sanguosha->getEngineCard(move.card_ids[i]);
                if (card->objectName() == objectName()) {
                    player->setFlags("-peacespell_throwing");
                    return QStringList(objectName());
                }
            }
        }
        return QStringList();
    }

    virtual bool effect(TriggerEvent triggerEvent, Room *room, ServerPlayer *player, QVariant &data, ServerPlayer *) const{
        if (triggerEvent == DamageInflicted){
            DamageStruct damage = data.value<DamageStruct>();

            LogMessage l;
            l.type = "#PeaceSpellNatureDamage" ;
            l.from = damage.from ;
            l.to << damage.to ;
            l.arg = QString::number(damage.damage);
            switch (damage.nature) {
            case DamageStruct::Normal: l.arg2 = "normal_nature"; break;
            case DamageStruct::Fire: l.arg2 = "fire_nature"; break;
            case DamageStruct::Thunder: l.arg2 = "thunder_nature"; break;
            }

            room->sendLog(l);
            room->setEmotion(damage.to, "armor/peacespell");

            return true;
        }
        else {
            LogMessage l;
            l.type = "#PeaceSpellLost" ;
            l.from = player ;

            room->sendLog(l);

            room->loseHp(player);
            if (player->isAlive())
                player->drawCards(2);
        }
        return false;
    }
};

class PeaceSpellSkillMaxCards: public MaxCardsSkill{
public:
    PeaceSpellSkillMaxCards(): MaxCardsSkill("#PeaceSpell-max"){

    }

    virtual int getExtra(const Player *target) const{
        if (!target->hasShownOneGeneral())
            return 0;

        QList<const Player *> targets = target->getAliveSiblings();
        targets << target;

        const Player *ps_owner = NULL;
        foreach (const Player *p, targets){
            if (p->hasArmorEffect("PeaceSpell")){
                ps_owner = p;
                break;
            }
        }

        if (ps_owner == NULL)
            return 0;

        if (target->isFriendWith(ps_owner))
            return ps_owner->getPlayerNumWithSameKingdom();

        return 0;
    }
};

MomentumEquipPackage::MomentumEquipPackage(): Package("momentum_equip", CardPack){
    PeaceSpell *dp = new PeaceSpell;
    dp->setParent(this);

    skills << new PeaceSpellSkill << new PeaceSpellSkillMaxCards;
    related_skills.insertMulti("PeaceSpell", "#PeaceSpell-max");
}

ADD_PACKAGE(MomentumEquip)
