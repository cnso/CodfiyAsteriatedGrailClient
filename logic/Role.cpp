﻿#include "Role.h"
#include <QStringList>
#include <QSound>
#include "data/DataInterface.h"
#include "widget/GUI.h"
#include "logic/Logic.h"

Role::Role(QObject *parent) :
    QObject(parent)
{
    Player*myself=dataInterface->getMyself();
    myID=myself->getID();
    int myColor=myself->getColor();

    network::GameInfo* gameInfo=dataInterface->getGameInfo();
    int playerMax=dataInterface->getPlayerMax();
    red=dataInterface->getRedTeam();
    blue=dataInterface->getBlueTeam();
    playerList=dataInterface->getPlayerList();
    int i;
    //find myPos
    for(i=0;i<playerMax;i++)
        if(gameInfo->player_infos(i).id()==myID)
            break;
    int ptr;
    do
    {
        i++;
        if(i==playerMax)
            i=0;
        ptr=gameInfo->player_infos(i).id();

    }while(playerList[ptr]->getColor()==myColor);
    nextCounterClockwise=ptr;
    handArea=gui->getHandArea();
    playerArea=gui->getPlayerArea();
    buttonArea=gui->getButtonArea();
    decisionArea=gui->getDecisionArea();
    tipArea=gui->getTipArea();
    teamArea=gui->getTeamArea();
    coverArea = gui->getCoverArea();    
    showArea=gui->getShowArea();

}
void Role::makeConnection()
{
    connect(logic->getClient(),SIGNAL(getMessage(uint16_t, google::protobuf::Message*)),this,SLOT(decipher(uint16_t, google::protobuf::Message*)));
    connect(this,SIGNAL(sendCommand(uint16_t, google::protobuf::Message*)),logic->getClient(),SLOT(sendMessage(uint16_t, google::protobuf::Message*)));
    connect(decisionArea,SIGNAL(okClicked()),this,SLOT(onOkClicked()));
    connect(decisionArea,SIGNAL(cancelClicked()),this,SLOT(onCancelClicked()));
    connect(decisionArea,SIGNAL(exchangeClicked()),this,SLOT(exchangeCards()));
    connect(decisionArea,SIGNAL(resignClicked()),this,SLOT(resign()));
    connect(buttonArea->getButtons().at(0),SIGNAL(buttonSelected(int)),this,SLOT(buy()));
    connect(buttonArea->getButtons().at(1),SIGNAL(buttonSelected(int)),this,SLOT(synthetize()));
    connect(buttonArea->getButtons().at(2),SIGNAL(buttonSelected(int)),this,SLOT(extract()));
    connect(buttonArea,SIGNAL(buttonUnselected()),this,SLOT(onCancelClicked()));
    connect(handArea,SIGNAL(cardReady()),this,SLOT(cardAnalyse()));
    connect(coverArea,SIGNAL(cardReady()),this,SLOT(coverCardAnalyse()));
    connect(playerArea,SIGNAL(playerReady()),this,SLOT(playerAnalyse()));
}

void Role::coverCardAnalyse()
{
    switch(state)
    {
    case 50:
        decisionArea->enable(0);
        break;
    }
}

void Role::cardAnalyse()
{
    int i;
    QList<Card*> selectedCards=handArea->getSelectedCards();
    QString cardName;
    switch (state)
    {
//normal action
    case 1:
//追加攻击或法术行动
    case 11:
    case 12:
        if(selectedCards[0]->getType()=="magic" )
        {
            cardName=selectedCards[0]->getName();
            if(cardName==QStringLiteral("中毒"))
                playerArea->enableAll();
            if(cardName==QStringLiteral("虚弱"))
            {
                playerArea->reset();
                for(i=0;i<dataInterface->getPlayerMax();i++)
                    if(!dataInterface->getPlayerList().at(i)->checkBasicStatus(1))
                        playerArea->enablePlayerItem(i);
            }
            if(cardName==QStringLiteral("圣盾"))
            {
                playerArea->reset();
                for(i=0;i<dataInterface->getPlayerMax();i++)
                    if(!dataInterface->getPlayerList().at(i)->checkBasicStatus(2))
                        playerArea->enablePlayerItem(i);
            }
            if(cardName==QStringLiteral("魔弹"))
            {
                playerArea->reset();
                playerArea->enablePlayerItem(nextCounterClockwise);
            }
        }
        else
    case 10:
            setAttackTarget();
    break;
//attacked reply
    case 2:
        if(selectedCards[0]->getType()=="attack" )
        {
            playerArea->reset();
            playerArea->enableEnemy();
            playerArea->disablePlayerItem(sourceID);
        }
        if(selectedCards[0]->getElement()=="light")
        {
            playerArea->reset();
            decisionArea->enable(0);
        }
    break;
//drop cards
    case 3:
//天使祝福给牌
    case 751:
//魔爆冲击弃牌,充盈
    case 851:
    case 2951:
        decisionArea->enable(0);
    break;
//魔弹reply
    case 8:
        cardName=selectedCards[0]->getName();
        if(cardName==QStringLiteral("魔弹"))
        {
            playerArea->reset();
            playerArea->enablePlayerItem(moDanNextID);
        }
        else if(cardName==QStringLiteral("圣光"))
        {
            playerArea->reset();
            decisionArea->enable(0);
        }
        break;
    }
}

void Role::playerAnalyse()
{
    decisionArea->enable(0);
}

void Role::setAttackTarget()
{
    playerArea->reset();
    playerArea->enableEnemy();
//潜行
    for(int i=0;i<playerList.size();i++)
        if(playerList[i]->getRoleID()==5 && playerList[i]->getTap()==1){
            playerArea->disablePlayerItem(i);
            break;
        }
//挑衅
    if(playerList[myID]->getSpecial(1) == 1)
        for(int i=0;i<playerList.size();i++)
            if(playerList[i]->getRoleID()!=21)
                playerArea->disablePlayerItem(i);

}

void Role::exchangeCards()
{
    network::Action* action = newAction(30);
    QList<Card*> handcards=dataInterface->getHandCards();
    int i;
    int n=handcards.count();
    for(i=0;i<n;i++)
        dataInterface->removeHandCard(handcards[i]);
    emit sendCommand(network::MSG_ACTION, action);
}

void Role::resign()
{
    network::Action* action = newAction(0);
    emit sendCommand(network::MSG_ACTION, action);
    gui->reset();
}

//正常行动
void Role::normal()
{
    gui->reset();

    QList<Card*> handcards=dataInterface->getHandCards();
    Player* myself=dataInterface->getMyself();
    Team* myTeam=dataInterface->getMyTeam();
    int n=handcards.count();

    state=1;
    actionFlag=0;
    playerArea->setQuota(1);
    handArea->setQuota(1);

    handArea->enableAll();
    handArea->disableElement("light");

    if(n+3 <= myself->getHandCardMax() && start==false)
    {
//购买
        buttonArea->enable(0);
        if(myTeam->getEnergy()>=3)
//合成
            buttonArea->enable(1);
    }
//提炼
    if(myself->getEnergy()<myself->getEnergyMax() && myTeam->getEnergy()>0 && start==false)
        buttonArea->enable(2);
    tipArea->setMsg(QStringLiteral("轮到你了"));
    unactionalCheck();
}

//无法行动检测
void Role::unactionalCheck()
{
    bool actional=false;
    QList<Button*>buttons=buttonArea->getButtons();
    QList<CardItem*>cardItems=handArea->getHandCardItems();
    for(int i=0;i<buttons.size();i++)
        if(buttons[i]->isEnabled()&&i!=2)
        {
            actional=true;
            break;
        }
    for(int i=0;i<cardItems.size()&&!actional;i++)
        if(cardItems[i]->isEnabled())
        {
            actional=true;
            break;
        }
    if(!actional)
        decisionArea->enable(2);
    else
        decisionArea->disable(2);
}

void Role::attackAction()
{
    gui->reset();

    state=10;
    actionFlag=1;
    playerArea->setQuota(1);
    handArea->setQuota(1);
    handArea->enableAttack();
    decisionArea->enable(3);
}

void Role::magicAction()
{
    gui->reset();

    state=11;
    actionFlag=2;
    playerArea->setQuota(1);
    handArea->setQuota(1);
    handArea->enableMagic();
    handArea->disableElement("light");
    decisionArea->enable(3);

}

void Role::attackOrMagic()
{
    gui->reset();

    state=12;
    actionFlag=4;
    playerArea->setQuota(1);
    handArea->setQuota(1);
    handArea->enableAll();
    handArea->disableElement("light");
    decisionArea->enable(3);
}

void Role::attacked(QString element,int hitRate)
{
    state=2;
    playerArea->setQuota(1);
    handArea->setQuota(1);

    decisionArea->enable(1);
    if(hitRate==0)
    {
        handArea->enableElement(element);
        handArea->enableElement("darkness");
    }
    handArea->disableMagic();
    handArea->enableElement("light");
    gui->alert();
    QSound::play("sound/Warning.wav");
}

void Role::drop(int howMany)
{
    state=3;
    handArea->setQuota(howMany);
    handArea->enableAll();
    gui->alert();

}

void Role::dropCover(int howMany)
{
    state = 50;
    gui->showCoverArea(true);
    HandArea *coverArea = gui->getCoverArea();
    coverArea->reset();
    coverArea->setQuota(howMany);
    coverArea->enableAll();
    gui->alert();

}

void Role::buy()
{
    Team*team=dataInterface->getMyTeam();

    int energy=team->getEnergy();

    state=4;
    decisionArea->enable(0);
    decisionArea->enable(1);
    handArea->reset();
    playerArea->reset();
    tipArea->reset();

    if(energy<4)
        tipArea->setMsg(QStringLiteral("你摸3张牌，你方战绩区加一宝石一水晶"));
    if(energy==4)
    {
        tipArea->setMsg(QStringLiteral("战绩区星石已有4个，请选择要购买的星石："));
        tipArea->addBoxItem(QStringLiteral("1.一个宝石"));
        tipArea->addBoxItem(QStringLiteral("2.一个水晶"));
        tipArea->showBox();
    }
    if(energy==5)
    {
        tipArea->setMsg(QStringLiteral("战绩区星石数目已达上限，购买将不再增加星石"));
    }
}

void Role::synthetize()
{
    int gem,crystal;
    Team* team;

    team=dataInterface->getMyTeam();
    gem=team->getGem();
    crystal=team->getCrystal();

    state=5;
    decisionArea->enable(0);
    decisionArea->enable(1);
    tipArea->reset();
    handArea->reset();
    playerArea->reset();

    tipArea->setMsg(QStringLiteral("请选择用来合成的星石："));
    if(crystal>=3)
        tipArea->addBoxItem(QStringLiteral("1.三个水晶"));
    if(crystal>=2&&gem>=1)
        tipArea->addBoxItem(QStringLiteral("2.两个水晶和一个宝石"));
    if(crystal>=1&&gem>=2)
        tipArea->addBoxItem(QStringLiteral("3.一个水晶和两个宝石"));
    if(gem>=3)
        tipArea->addBoxItem(QStringLiteral("4.三个宝石"));
    tipArea->showBox();
}

void Role::extract()
{
    int gem,crystal;
    Team* team;
    Player* myself=dataInterface->getPlayerList().at(myID);

    team=dataInterface->getMyTeam();
    gem=team->getGem();
    crystal=team->getCrystal();

    state=6;
    decisionArea->enable(0);
    decisionArea->enable(1);
    tipArea->reset();
    handArea->reset();
    playerArea->reset();

    tipArea->setMsg(QStringLiteral("请选择要提取的星石数："));
    switch(myself->getEnergyMax()-myself->getEnergy())
    {
    case 4:
    case 3:
    case 2:
        if(gem>=2)
            tipArea->addBoxItem(QStringLiteral("1.两个宝石"));
        if(crystal>=2)
            tipArea->addBoxItem(QStringLiteral("2.两个水晶"));
        if(gem>=1&&crystal>=1)
            tipArea->addBoxItem(QStringLiteral("3.一个宝石和一个水晶"));
    case 1:
        if(gem>=1)
            tipArea->addBoxItem(QStringLiteral("4.一个宝石"));
        if(crystal>=1)
            tipArea->addBoxItem(QStringLiteral("5.一个水晶"));
    }
    tipArea->showBox();
}

void Role::moDaned(int nextID,int sourceID,int howMany)
{
    state=8;
    gui->reset();
    QString msg=dataInterface->getPlayerList().at(sourceID)->getRoleName()+QStringLiteral("对")+QStringLiteral("你")+QStringLiteral("使用了魔弹，目前伤害为：")+QString::number(howMany)+QStringLiteral("点");
    tipArea->setMsg(msg);
    playerArea->setQuota(1);
    handArea->setQuota(1);

    handArea->enableElement("light");
    handArea->enableMoDan();
    decisionArea->enable(1);
    moDanNextID=nextID;
    gui->alert();
    QSound::play("sound/Warning.wav");
}

void Role::cure(int cross,int harmPoint, int type, int crossAvailable)
{
    int min=crossAvailable<harmPoint?crossAvailable:harmPoint;
    QString msg=QStringLiteral("你受到")+QString::number(harmPoint)+QStringLiteral("点");
    if(type==1)
        msg+=QStringLiteral("攻击");
    else
        msg+=QStringLiteral("法术");
    msg+=QStringLiteral("伤害，要使用多少个治疗抵御之？");

    state=9;
    decisionArea->enable(0);
    decisionArea->enable(1);

    tipArea->setMsg(msg);
    for(;min>=0;min--)
        tipArea->addBoxItem(QString::number(min));
    tipArea->showBox();
    gui->alert();
    QSound::play("sound/Warning.wav");
}

void Role::turnBegin()
{
    isMyTurn=true;
    onceUsed=false;
    start=false;
    usedAttack=usedMagic=usedSpecial=false;
    gui->alert();
    QSound::play("sound/Warning.wav");
}

void Role::additionalAction(){
    gui->reset();
    tipArea->setMsg(QStringLiteral("是否执行额外行动？"));
    if(dataInterface->getMyself()->checkBasicStatus(5))
        gui->getTipArea()->addBoxItem("0.迅捷赐福");
    state=42;
    tipArea->showBox();
    decisionArea->enable(0);
    decisionArea->enable(3);
    gui->alert();
    QSound::play("sound/Warning.wav");
}

void Role::askForSkill(QString skill)
{
    gui->reset();
    if(skill==QStringLiteral("威力赐福"))
        WeiLi();
    gui->alert();
    QSound::play("sound/Warning.wav");
}

void Role::TianShiZhuFu(int n)
{
    gui->reset();
    state=751;
    tipArea->setMsg(QStringLiteral("给予天使")+QString::number(n)+QStringLiteral("张牌"));
    handArea->setQuota(n);
    handArea->enableAll();
    gui->alert();
    QSound::play("sound/Warning.wav");
}

void Role::MoBaoChongJi()
{
    gui->reset();
    state=851;
    tipArea->setMsg(QStringLiteral("弃一张法术牌或受到两点法术伤害"));
    handArea->setQuota(1);
    handArea->enableMagic();
    decisionArea->enable(1);
    gui->alert();
    QSound::play("sound/Warning.wav");
}

void Role::WeiLi()
{
    state=36;
    tipArea->setMsg(QStringLiteral("是否发动威力赐福？"));
    decisionArea->enable(0);
    decisionArea->enable(1);
}

void Role::ChongYing(int color)
{
    gui->reset();
    state=2951;
    Player* myself=dataInterface->getMyself();
    if(myself->getRoleName()!=QStringLiteral("[魔枪]"))
        tipArea->setMsg(QStringLiteral("弃一张牌，法术或雷将会为魔枪加1伤害"));
    else
        tipArea->setMsg(QStringLiteral("可弃一张牌"));
    if(handArea->getHandCardItems().size()==0)
        decisionArea->enable(3);
    else if(color==myself->getColor())
    {
        decisionArea->enable(1);
        handArea->enableAll();
        handArea->setQuota(1);
    }
    else
    {
        handArea->enableAll();
        handArea->setQuota(1);
    }
    decisionArea->disable(0);
    gui->alert();
}

void Role::onCancelClicked()
{
    QString command;

    network::Respond* respond;
    switch(state)
    {
//normal
    case 1:
    case 4:
    case 5:
    case 6:
        gui->reset();
        myRole->normal();
        break;
//ATTACKEDREPLY
    case 2:
        respond = newRespond(network::RESPOND_REPLY_ATTACK);
        respond->add_args(2);
        emit sendCommand(network::MSG_RESPOND, respond);
        gui->reset();
        break;
//虚弱
    case 7:
        respond = newRespond(network::RESPOND_WEAKEN);
        respond->add_args(1);
        emit sendCommand(network::MSG_RESPOND, respond);
        gui->reset();
        break;
//魔弹应答
    case 8:
        respond = newRespond(network::RESPOND_BULLET);
        respond->add_args(2);
        respond->add_args(100000);
        emit sendCommand(network::MSG_RESPOND, respond);
        gui->reset();
        break;
//治疗应答
    case 9:
        respond = newRespond(network::RESPOND_HEAL);
        respond->add_args(0);
        emit sendCommand(network::MSG_RESPOND, respond);
        gui->reset();
        break;
//简单的技能发动询问
    case 36:
        respond = newRespond(36);
        emit sendCommand(network::MSG_RESPOND, respond);
        gui->reset();
        break;
//魔爆冲击弃牌
    case 851:
        respond = newRespond(851);
        emit sendCommand(network::MSG_RESPOND, respond);
        gui->reset();
        break;
//充盈弃牌
    case 2951:
        respond = newRespond(2951);
        emit sendCommand(network::MSG_RESPOND, respond);
        gui->reset();
        break;
    }
}

void Role::onOkClicked()
{
    QList<Card*>selectedCards;
    QList<Player*>selectedPlayers;

    QString command;
    QString cardID;
    QString sourceID;
    QString targetID;
    QString text;
    int i,boxCurrentIndex;

    selectedCards=handArea->getSelectedCards();
    selectedPlayers=playerArea->getSelectedPlayers();

    network::Action* action;
    network::Respond* respond;
    switch(state)
    {
//NORMALACTION
    case 1:
//追加行动
    case 10:
    case 11:
    case 12:
        if(selectedCards[0]->getType()=="attack"){
            action = newAction(network::ACTION_ATTACK);
            action->add_args(selectedCards[0]->getID());
            action->add_dst_ids(selectedPlayers[0]->getID());
            usedAttack=true;
            usedMagic=usedSpecial=false;
        }
        else
        {
            QString cardName;
            cardName=selectedCards[0]->getName();
            if(cardName==QStringLiteral("中毒")||cardName==QStringLiteral("虚弱")||cardName==QStringLiteral("圣盾")||cardName==QStringLiteral("魔弹"))
            {
                action = newAction(network::ACTION_MAGIC);
                action->add_args(selectedCards[0]->getID());
                action->add_dst_ids(selectedPlayers[0]->getID());
            }
            usedMagic=true;
            usedAttack=usedSpecial=false;
        }
        dataInterface->removeHandCard(selectedCards[0]);
        gui->reset();
        emit sendCommand(network::MSG_ACTION, action);
        break;
//ATTACKEDREPLY
    case 2:
        respond = newRespond(network::RESPOND_REPLY_ATTACK);

        if(selectedCards[0]->getType()=="attack")
        {
            respond->add_args(0);
            respond->add_args(selectedCards[0]->getID());
            respond->add_dst_ids(selectedPlayers[0]->getID());
        }
        else if(selectedCards[0]->getElement()=="light")
        {
            respond->add_args(1);
            respond->add_args(selectedCards[0]->getID());
        }
        dataInterface->removeHandCard(selectedCards[0]);
        gui->reset();
        emit sendCommand(network::MSG_RESPOND, respond);
        break;
//DROPREPLY
    case 3:
        respond = newRespond(network::RESPOND_DISCARD);

        for(i=1;i<selectedCards.count();i++)
        {
            respond->add_args(selectedCards[i]->getID());
        }
        command+=";";
        for(i=0;i<selectedCards.count();i++)
        {
            dataInterface->removeHandCard(selectedCards[i]);
        }
        gui->reset();
        emit sendCommand(network::MSG_RESPOND, respond);
        break;
//BUY
    case 4:
        action = newAction(network::ACTION_BUY);
        boxCurrentIndex=tipArea->getBoxCurrentIndex();

        switch(boxCurrentIndex)
        {
        case -1:
            if(dataInterface->getMyTeam()->getEnergy()<4)
            {
                action->add_args(1);
                action->add_args(1);
            }
            else
            {
                action->add_args(0);
                action->add_args(0);
            }
            break;
        case 0:
            action->add_args(1);
            action->add_args(0);
            break;
        case 1:
            action->add_args(0);
            action->add_args(1);
            break;
        }
        gui->reset();
        usedSpecial=true;
        usedAttack=usedMagic=false;
        emit sendCommand(network::MSG_ACTION, action);
        break;
//SYNTHETIZE
    case 5:
        action = newAction(network::ACTION_COMPOSE);

        text=tipArea->getBoxCurrentText();
        switch(text[0].digitValue())
        {
        case 1:
            action->add_args(0);
            action->add_args(3);
            break;
        case 2:
            action->add_args(1);
            action->add_args(2);
            break;
        case 3:
            action->add_args(2);
            action->add_args(1);
            break;
        case 4:
            action->add_args(3);
            action->add_args(0);
            break;
        }
        gui->reset();
        usedSpecial=true;
        usedAttack=usedMagic=false;
        emit sendCommand(network::MSG_ACTION, action);
        break;
//EXTRACT
    case 6:
        action = newAction(network::ACTION_REFINE);

        text=tipArea->getBoxCurrentText();
        switch(text[0].digitValue())
        {
        case 1:
            action->add_args(2);
            action->add_args(0);
            break;
        case 2:
            action->add_args(0);
            action->add_args(2);
            break;
        case 3:
            action->add_args(1);
            action->add_args(1);
            break;
        case 4:
            action->add_args(1);
            action->add_args(0);
            break;
        case 5:
            action->add_args(0);
            action->add_args(1);
            break;
        }
        gui->reset();
        usedSpecial=true;
        usedAttack=usedMagic=false;
        emit sendCommand(network::MSG_ACTION, action);
        break;
//虚弱
    case 7:
        respond = newRespond(network::RESPOND_WEAKEN);
        respond->add_args(0);
        gui->reset();
        emit sendCommand(network::MSG_RESPOND, respond);
        break;
//魔弹应答
    case 8:
        respond = newRespond(network::RESPOND_BULLET);
        if(selectedCards[0]->getName()==QStringLiteral("圣光"))
        {
            respond->add_args(selectedCards[0]->getID());
        }
        else if(selectedCards[0]->getName()==QStringLiteral("魔弹"))
        {
            respond->add_args(selectedCards[0]->getID());
            respond->add_dst_ids(selectedPlayers[0]->getID());
        }
        dataInterface->removeHandCard(selectedCards[0]);
        gui->reset();
        emit sendCommand(network::MSG_RESPOND, respond);
        break;
//治疗应答
    case 9:
        respond = newRespond(network::RESPOND_HEAL);
        text=tipArea->getBoxCurrentText();
        respond->add_args(text.toInt());
        gui->reset();
        emit sendCommand(network::MSG_RESPOND, respond);
        break;
//简单的技能发动询问
    case 36:
        respond = newRespond(36);
        respond->add_args(1);
        emit sendCommand(network::MSG_RESPOND, respond);
        gui->reset();
        break;
//额外行动（迅捷）
    case 42:
        if(tipArea->getBoxCurrentText().at(0).digitValue()==0)
        {
            respond = newRespond(1607);
            respond->add_args(1);
            emit sendCommand(network::MSG_RESPOND, respond);
            myRole->attackAction();
        }
        break;
//弃盖牌
    case 50:
        respond = newRespond(50);

        selectedCards = coverArea->getSelectedCards();
        for(i=0;i<selectedCards.count();i++)
        {
            respond->add_args(selectedCards[i]->getID());
        }
        command+=";";
        for(i=0;i<selectedCards.count();i++)
        {
            dataInterface->removeHandCard(selectedCards[i]);
        }
        coverArea->reset();
        gui->showCoverArea(false);
        gui->reset();
        emit sendCommand(network::MSG_RESPOND, respond);
        break;
//天使祝福
    case 751:
        respond = newRespond(751);
        respond->add_args(selectedCards[0]->getID());
        dataInterface->removeHandCard(selectedCards[0]);
        if(selectedCards.size()==2)
        {
            respond->add_args(selectedCards[1]->getID());
            dataInterface->removeHandCard(selectedCards[1]);
        }
        gui->reset();
        emit sendCommand(network::MSG_RESPOND, respond);
        break;
//魔爆冲击
    case 851:
        respond = newRespond(851);
        respond->add_args(selectedCards[0]->getID());
        dataInterface->removeHandCard(selectedCards[0]);
        gui->reset();
        emit sendCommand(network::MSG_RESPOND, respond);
        break;
//充盈
    case 2951:
        respond = newRespond(2951);
        respond->add_args(selectedCards[0]->getID());

        dataInterface->removeHandCard(selectedCards[0]);
        gui->reset();
        emit sendCommand(network::MSG_RESPOND, respond);
        break;
    }
}

network::Action* Role::newAction(uint32_t action_id)
{
    network::Action* action = new network::Action();
    action->set_src_id(myID);
    action->set_action_id(action_id);
    return action;
}

network::Respond* Role::newRespond(uint32_t respond_id)
{
    network::Respond* respond = new network::Respond();
    respond->set_src_id(myID);
    respond->set_respond_id(respond_id);
    return respond;
}

void Role::decipher(quint16 proto_type, google::protobuf::Message* proto)
{
    this->command=command;
    QStringList arg=command.split(';');
    QStringList cardIDList;
    int targetID,targetArea;
    int sourceArea;
    int cardID;
    int hitRate;
    int i,howMany;
    int team,gem,crystal;
    int dir,show;

    Card*card;
    Player*player;    
    QList<Card*> cards;
    QString msg;
    QString flag;
    QString cardName;


    switch (proto_type)
    {
    case network::MSG_TURN_BEGIN:
        //回合开始
        targetID=((network::TurnBegin*)proto)->id();
        gui->logAppend("--------------------------------------");
        gui->logAppend(playerList[targetID]->getRoleName()+QStringLiteral("回合开始"));
        playerArea->setCurrentPlayerID(targetID);

        if(targetID==dataInterface->getFirstPlayerID())
            teamArea->addRoundBy1();
        if(targetID!=myID)
        {
            isMyTurn=0;
            gui->setEnable(0);
        }
        else
        {
            myRole->turnBegin();
            QSound::play("sound/It's your turn.wav");
        }

        break;

    case network::MSG_CMD_REQ:
        // 应战询问
    {
        network::CommandRequest* cmd_req = (network::CommandRequest*)proto;
        for (int i = 0; i < cmd_req->commands_size(); ++i)
        {
            network::Command* cmd = (network::Command*)&(cmd_req->commands(i));
            switch (cmd->respond_id())
            {
            case network::RESPOND_REPLY_ATTACK:
                hitRate=cmd->args(0);
                cardID=cmd->args(1);
                targetID=cmd->args(2);
                sourceID=cmd->args(3);
                card=dataInterface->getCard(cardID);
                QSound::play("sound/Attack.wav");

                if(targetID!=myID)
                {
                    gui->setEnable(0);
                }
                else
                {
                    gui->setEnable(1);
                    msg=playerList[sourceID]->getRoleName()+QStringLiteral("对")+QStringLiteral("你")+QStringLiteral("使用了")+card->getName();
                    if (hitRate==2)
                        return;

                    if(hitRate==1)
                        msg+=QStringLiteral("，该攻击无法应战");

                    gui->reset();
                    tipArea->setMsg(msg);
                    myRole->attacked(card->getElement(),hitRate);
                }
                break;
            case network::RESPOND_DISCARD:
                //弃牌询问
                targetID=cmd->args(0);
                howMany=cmd->args(1);
                char str[10];
                sprintf(str, "%d", howMany);

                msg=playerList[targetID]->getRoleName()+QStringLiteral("需要弃")+QString(str)+QStringLiteral("张牌");
                if(cmd->args(2) == 1)
                    gui->logAppend(msg+QStringLiteral("明弃"));
                else
                    gui->logAppend(msg+QStringLiteral("暗弃"));
                if(targetID!=myID)
                {
                    gui->setEnable(0);
                }
                else
                {
                    gui->setEnable(1);
                    gui->reset();
                    drop(howMany);
                    tipArea->setMsg(QStringLiteral("你需要弃")+QString(str)+QStringLiteral("张牌"));
                }
                break;
            case network::RESPOND_WEAKEN:
                // 虚弱询问
                targetID=cmd->args(0);
                gui->logAppend(playerList[targetID]->getRoleName()+QStringLiteral("被虚弱了"));
                if(targetID!=myID)
                {
                    gui->setEnable(0);
                }
                else
                {
                    gui->setEnable(1);
                    gui->reset();
                    state=7;
                    decisionArea->enable(0);
                    decisionArea->enable(1);
                    tipArea->setMsg(QStringLiteral("你被虚弱了，请选择跳过行动或者摸取")+arg[2]+QStringLiteral("张牌"));
                }
                break;
            case network::RESPOND_HEAL:
            {
                // 治疗询问
                targetID=cmd->args(0);
                howMany=cmd->args(1);
                int max_avail = cmd->args(3);
                gui->reset();
                if(targetID==myID)
                    myRole->cure(playerList[myID]->getCrossNum(),howMany,cmd->args(2),max_avail);
                break;
            }
            case network::RESPOND_BULLET:
            {
                int nextID;
                targetID=cmd->args(0);
                sourceID=cmd->args(1);
                howMany=cmd->args(2);
                nextID=cmd->args(3);
                char str[10];
                sprintf(str, "%d", howMany);
                gui->logAppend(playerList[sourceID]->getRoleName()+QStringLiteral("对")+playerList[targetID]->getRoleName()+QStringLiteral("使用了魔弹，目前伤害为：")+QString(str)+QStringLiteral("点"));
                if(targetID!=myID)
                {
                    gui->setEnable(0);
                }
                else
                {
                    gui->setEnable(1);
                    myRole->moDaned(nextID,sourceID,howMany);
                }
                break;
            }
            case network::ACTION_ANY:
                targetID = cmd->args(0);
                if(targetID!=myID)
                {
                    isMyTurn=0;
                    gui->setEnable(0);
                    actionFlag=-1;
                }
                else
                {
                    gui->setEnable(1);
                    myRole->normal();
                }
                break;
            case network::ACTION_ATTACK_MAGIC:
                if(targetID!=myID)
                {
                    isMyTurn=0;
                    gui->setEnable(0);
                    actionFlag=-1;
                }
                else
                {
                    gui->setEnable(1);
                    myRole->attackOrMagic();
                }
                break;
            case network::ACTION_ATTACK:
                if(targetID!=myID)
                {
                    isMyTurn=0;
                    gui->setEnable(0);
                    actionFlag=-1;
                }
                else
                {
                    gui->setEnable(1);
                    myRole->attackAction();
                }
                break;
            case network::ACTION_MAGIC:
                if(targetID!=myID)
                {
                    isMyTurn=0;
                    gui->setEnable(0);
                    actionFlag=-1;
                }
                else
                {
                    gui->setEnable(1);
                    myRole->magicAction();
                }
            case 750:
                // 天使祝福
                targetID=cmd->args(0);
                howMany=cmd->args(1);
                msg=QStringLiteral("等待")+playerList[targetID]->getRoleName()+QStringLiteral("天使祝福（给牌）响应");
                gui->logAppend(msg);
                if(targetID!=myID)
                {
                    isMyTurn=0;
                    gui->setEnable(0);
                }
                else
                {
                    gui->setEnable(1);
                    TianShiZhuFu(howMany);
                }
                break;
            case 850:
                // 魔爆冲击（弃牌）
                targetID=cmd->args(0);
                msg=QStringLiteral("等待")+playerList[targetID]->getRoleName()+QStringLiteral("魔爆冲击（弃牌）响应");
                gui->logAppend(msg);
                if(targetID!=myID)
                {
                    isMyTurn=0;
                    gui->setEnable(0);
                }
                else
                {
                    gui->setEnable(1);
                    MoBaoChongJi();
                }
                break;
            case 2950:
            {
                // 充盈（弃牌）
                targetID=cmd->args(0);
                int color=cmd->args(1);
                msg=QStringLiteral("等待")+playerList[targetID]->getRoleName()+QStringLiteral("充盈（弃牌）响应");
                gui->logAppend(msg);
                if(targetID!=myID)
                {
                    isMyTurn=0;
                    gui->setEnable(0);
                }
                else
                {
                    gui->setEnable(1);
                    ChongYing(color);
                }
                break;
            }
            default:
                // 其他技能
                msg=QStringLiteral("等待")+playerList[targetID]->getRoleName()+QStringLiteral("的技能响应");
                gui->logAppend(msg);
                if(targetID==myID)
                {
                    gui->setEnable(1);
                    // TODO:编号到技能名称转换或改成使用技能编号
                    //myRole->askForSkill(cmd->respond_id);
                }
                else
                    gui->setEnable(0);
                break;
            }
        }
        break;
    }

    case network::MSG_ACTION:
    {
        network::Action* action = (network::Action*)proto;

        switch (action->action_id())
        {
        case network::ACTION_ATTACK:
            cardID=action->args(0);
            targetID=action->dst_ids(0);
            sourceID=action->src_id();
            card=dataInterface->getCard(cardID);

            if(targetID!=-1 && targetID!=sourceID)
                playerArea->drawLineBetween(sourceID,targetID);

            cards.clear();
            cards<<card;
            showArea->showCards(cards);

            gui->logAppend(playerList[sourceID]->getRoleName()+QStringLiteral("对")+playerList[targetID]->getRoleName()+QStringLiteral("使用了<font color=\'orange\'>")+card->getInfo()+"</font>");
            break;
        case network::ACTION_BUY:
            break;
        case network::ACTION_COMPOSE:
            break;
        case network::ACTION_MAGIC:
            cardID=action->args(0);
            targetID=action->dst_ids(0);
            sourceID=action->src_id();
            card=dataInterface->getCard(cardID);

            if(targetID!=-1 && targetID!=sourceID)
                playerArea->drawLineBetween(sourceID,targetID);

            cards.clear();
            cards<<card;
            showArea->showCards(cards);

            gui->logAppend(playerList[sourceID]->getRoleName()+QStringLiteral("对")+playerList[targetID]->getRoleName()+QStringLiteral("使用了<font color=\'orange\'>")+card->getInfo()+"</font>");
            break;
        case network::ACTION_REFINE:
            break;
        case network::ACTION_NONE:
            targetID=action->src_id();
            msg=playerList[targetID]->getRoleName()+QStringLiteral("宣告无法行动");
            gui->logAppend(msg);
            break;
        default:
            break;
        }

        break;
    }
    case network::MSG_RESPOND:
    {
        network::Respond* respond = (network::Respond*)proto;

        switch (respond->respond_id())
        {
        case network::RESPOND_DISCARD:
            break;
        case network::RESPOND_BULLET:
            int nextID;
            targetID=respond->dst_ids(0);
            sourceID=respond->src_id();
            howMany=respond->args(0);
            nextID=respond->args(1);
            gui->logAppend(playerList[sourceID]->getRoleName()+QStringLiteral("对")+playerList[targetID]->getRoleName()+QStringLiteral("使用了魔弹，目前伤害为：")+QString::number(howMany)+QStringLiteral("点"));
            break;
        case network::RESPOND_REPLY_ATTACK:
            if (respond->args(0) == 2)
                gui->logAppend(playerList[sourceID]->getRoleName()+QStringLiteral("放弃应战和抵挡"));
            if (respond->args(0) != 2)
                cardID=respond->args(1);
            if (respond->args(0) == 0)
                targetID=respond->dst_ids(0);
            sourceID=respond->src_id();

            card=dataInterface->getCard(cardID);

            if(targetID!=-1 && targetID!=sourceID)
                playerArea->drawLineBetween(sourceID,targetID);

            cards.clear();
            cards<<card;
            showArea->showCards(cards);

            if(card->getElement()!="light")
                gui->logAppend(playerList[sourceID]->getRoleName()+QStringLiteral("对")+playerList[targetID]->getRoleName()+QStringLiteral("使用了<font color=\'orange\'>")+card->getInfo()+"</font>");
            else
                gui->logAppend(playerList[sourceID]->getRoleName()+"使用<font color=\'orange\'>"+card->getInfo()+"</font>抵挡");
            break;
        default:
            break;
        }

        break;
    }

    case network::MSG_HIT:
    // 命中通告
    {
        network::HitMsg* hit_msg = (network::HitMsg*)proto;
        targetID=hit_msg->dst_id();
        sourceID=hit_msg->src_id();
        if(hit_msg->hit())
            msg=playerList[sourceID]->getRoleName()+QStringLiteral("命中了")+playerList[targetID]->getRoleName();
        else
            msg=playerList[sourceID]->getRoleName()+QStringLiteral("未命中")+playerList[targetID]->getRoleName();
        gui->logAppend(msg);
        QSound::play("sound/Hit.wav");
        break;
    }
    case network::MSG_GOSSIP:
    // 对话及公告
    {
        network::Gossip* gossip = (network::Gossip*)proto;
        if (gossip->type() == network::GOSSIP_TALK)
            gui->chatAppend(gossip->id(), QString(gossip->txt().c_str()));
        break;
    }

    case network::MSG_GAME:
    // board更新
    {
        network::GameInfo* game_info = (network::GameInfo*)proto;

        int winner = -1;

        // 更新士气
        if (game_info->has_red_morale())
        {
            if (red->getMorale() > game_info->red_morale())
                QSound::play("sound/Hurt.wav");
            QSound::play("sound/Morale.wav");
            red->setMorale(game_info->red_morale());
            teamArea->update();
            if (red->getMorale() == 0)
                winner = 0;
        }
        if (game_info->has_blue_morale())
        {
            if (blue->getMorale() > game_info->blue_morale())
                QSound::play("sound/Hurt.wav");
            QSound::play("sound/Morale.wav");
            blue->setMorale(game_info->blue_morale());
            teamArea->update();
            if (blue->getMorale() == 0)
                winner = 1;
        }
        // 更新星杯
        if (game_info->has_red_grail())
        {
            red->setGrail(game_info->red_grail());
            teamArea->update();
            if (red->getGrail() == 5)
                winner = 1;
        }
        if (game_info->has_blue_grail())
        {
            blue->setGrail(game_info->blue_grail());
            teamArea->update();
            if (blue->getGrail() == 5)
                winner = 0;
        }

        // 胜负已分
        if (winner == 0 || winner == 1)
        {
            tipArea->win(winner);
            if(winner==dataInterface->getMyself()->getColor())
                QSound::play("sound/Win.wav");
            else
                QSound::play("sound/Lose.wav");
        }

        // 更新战绩区
        if (game_info->has_red_gem())
        {
            red->setGem(game_info->red_gem());
            QSound::play("sound/Stone.wav");
        }
        if (game_info->has_blue_gem())
        {
            blue->setGem(game_info->blue_gem());
            QSound::play("sound/Stone.wav");
        }
        if (game_info->has_red_crystal())
        {
            red->setGem(game_info->red_crystal());
            QSound::play("sound/Stone.wav");
        }
        if (game_info->has_blue_crystal())
        {
            blue->setGem(game_info->blue_crystal());
            QSound::play("sound/Stone.wav");
        }
        // 更新牌堆、弃牌堆
        if (game_info->has_pile())
        {
            teamArea->setLeftCardNum(game_info->pile());
        }
        if (game_info->has_discard())
        {
            if (game_info->discard()==0 && teamArea->getDroppedCardNum() != 0)
            {
                gui->logAppend(QStringLiteral("牌堆重洗"));
                QSound::play("sound/Shuffle.wav");
            }
            teamArea->setDroppedCardNum(game_info->discard());
        }
        // 更新展示区
        if (game_info->show_cards_size() > 0)
        {
            msg+=":<font color=\'orange\'>";
            cards.clear();
            for(i=0;i<game_info->show_cards_size();i++)
            {
                cardID=game_info->show_cards(i);
                card=dataInterface->getCard(cardID);
                msg+=card->getInfo()+" ";
                cards<<card;
            }
            msg+="</font>";
            showArea->showCards(cards);

        }
//        // 清空数组
//        if (game_info->delete_field_size() > 0)
//        {
//            for (int i = 0; i < game_info->delete_field_size(); ++i)
//            {
//                if (strcmp(game_info->delete_field(i).c_str(), "show_cards") == 0)
//                {
//                    cards.clear();
//                    showArea->showCards(cards);
//                }
//            }
//        }
        // 更新玩家信息
        if (game_info->player_infos_size() > 0)
        {
            network::SinglePlayerInfo* player_info;
            for (int i = 0; i < game_info->player_infos_size(); ++i)
            {
                player_info = (network::SinglePlayerInfo*)&(game_info->player_infos(i));
                // TODO:
                targetID = player_info->id();

                player = playerList[targetID];

                // 更新手牌
                if (player_info->has_hand_count())
                {
                    if(player_info->hand_count()!=player->getHandCardNum())
                    {
                        gui->logAppend(player->getRoleName()+QStringLiteral("的手牌变为")+QString::number(player_info->hand_count())+QStringLiteral("张"));
                        player->changeHandCardNumTo(player_info->hand_count());
                    }
                    if (targetID == myID)
                    {
                        dataInterface->cleanHandCard();
                        for (int k = 0; k < player_info->hand_count(); ++k)
                        {
                            cardID=player_info->hands(k);
                            card=dataInterface->getCard(cardID);
                            dataInterface->addHandCard(card);
                        }
                    }
                }
                // 更新盖牌
                if (player_info->has_covered_count())
                {
                    char str[10];
                    sprintf(str, "%d", player_info->covered_count());
                    QString q_str(str);
                    gui->logAppend(player->getRoleName()+QStringLiteral("的盖牌变为")+q_str+QStringLiteral("张"));

                    if (targetID == myID)
                    {
                        dataInterface->cleanCoverCard();
                        for (int k = 0; k < player_info->covered_count(); ++k)
                            cardID=player_info->covereds(k);
                            card=dataInterface->getCard(cardID);
                            dataInterface->addCoverCard(card);
                    }
                    else
                    {
                        player->changeCoverCardNumTo(player_info->hand_count());
                    }
                }
                // 更新治疗
                if (player_info->has_heal_count())
                {
                    if (player->getCrossNum() > player_info->heal_count())
                    {
                       msg=playerList[targetID]->getRoleName()+QStringLiteral("æ²»ç–—å‡å°‘")+QString::number(howMany);
                       gui->logAppend(msg);
                    }
                    player->setCrossNum(player_info->heal_count());

                    playerArea->update();
                    QSound::play("sound/Cure.wav");
                }
                // 更新专属
                if (player_info->my_ex_card_place_size() > 0)
                {
                    // TODO:更新专属
                }
                if (player_info->gain_ex_card_size() > 0)
                {
                    player->cleanSpecial();
                    for (int j = 0; j < player_info->gain_ex_card_size(); ++j)
                    {
                        player->setSpecial(player_info->gain_ex_card(j), 1);
                    }
                }
                // 更新基础效果
                if (player_info->basic_cards_size() > 0)
                {
                    if (player_info->basic_cards_size() > player->getBasicStatus().size())
                        QSound::play("sound/Equip.wav");

                    player->cleanBasicStatus();
                    for (int j = 0; j < player_info->basic_cards_size(); ++j)
                    {
                        cardID=player_info->basic_cards(i);
                        card=dataInterface->getCard(cardID);
                        cardName=card->getName();
                        if(cardName==QStringLiteral("中毒"))
                            player->addBasicStatus(0,card);
                        if(cardName==QStringLiteral("虚弱"))
                            player->addBasicStatus(1,card);
                        if(cardName==QStringLiteral("圣盾")||card->getSpecialityList().contains(QStringLiteral("天使之墙")))
                            player->addBasicStatus(2,card);
                        if(card->getType()=="attack"&&card->getProperty()==QStringLiteral("幻"))
                            player->addBasicStatus(3,card);
                        if(card->getSpecialityList().contains(QStringLiteral("威力赐福")))
                            player->addBasicStatus(4,card);
                        if(card->getSpecialityList().contains(QStringLiteral("迅捷赐福")))
                            player->addBasicStatus(5,card);
                    }
                    break;
                }
                // 更新能量
                if (player_info->has_gem())
                {
                    player->setGem(player_info->gem());
                }
                if (player_info->has_crystal())
                {
                    player->setCrystal(player_info->crystal());
                }
                // 更新标记物
                if (player_info->has_yellow_energy())
                {
                    player->setToken(0, player_info->yellow_energy());
                }
                if (player_info->has_blue_energy())
                {
                    player->setToken(0, player_info->blue_energy());
                }
                // 更新玩家横置状态
                if (player_info->has_is_knelt())
                {
                    // 横置
                    if(!player_info->is_knelt())
                    {
                        playerList[targetID]->setTap(0);
                        msg=playerList[targetID]->getRoleName()+QStringLiteral("重置");
                    }
                    else
                    {
                        playerList[targetID]->setTap(1);
                        msg=playerList[targetID]->getRoleName()+QStringLiteral("横置");
                    }
                    playerArea->update();
                    gui->logAppend(msg);
                    break;
                }
                // 最大手牌改变
                if (player_info->has_max_hand())
                {
                    howMany=player_info->max_hand();
                    player->setHandCardsMax(howMany);
                }
//                // 清空数组
//                if (player_info->delete_field_size() > 0)
//                {
//                    for (int j = 0; j < player_info->delete_field_size(); ++j)
//                    {
//                        if (strcmp(player_info->delete_field(i).c_str(), "my_ex_card_place") == 0)
//                        {
//                            // TODO:清空专属
//                        }
//                        else if (strcmp(player_info->delete_field(i).c_str(), "gain_ex_card") == 0)
//                        {
//                            player->cleanSpecial();
//                        }
//                        else if (strcmp(player_info->delete_field(i).c_str(), "basic_cards") == 0)
//                        {
//                            player->cleanBasicStatus();
//                        }
//                    }
//                }
                playerArea->update();
            }
        }
    }
    default:
        break;
    }
}

