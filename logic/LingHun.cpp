﻿#include "LingHun.h"

LingHun::LingHun()
{

    makeConnection();
    setMyRole(this);
    lianJieUsed=false;
    Button *linghunzhaohuan;
    linghunzhaohuan=new Button(3,"灵魂召还");
    buttonArea->addButton(linghunzhaohuan);
    connect(linghunzhaohuan,SIGNAL(buttonSelected(int)),this,SLOT(LingHunZhaoHuan()));

    Button *linghunjingxiang;
    linghunjingxiang=new Button(4,"灵魂镜像");
    buttonArea->addButton(linghunjingxiang);
    connect(linghunjingxiang,SIGNAL(buttonSelected(int)),this,SLOT(LingHunJingXiang()));

    Button *linghunzhenbao;
    linghunzhenbao=new Button(5,"灵魂震爆");
    buttonArea->addButton(linghunzhenbao);
    connect(linghunzhenbao,SIGNAL(buttonSelected(int)),this,SLOT(LingHunZhenBao()));

    Button *linghunciyu;
    linghunciyu=new Button(6,"灵魂赐予");
    buttonArea->addButton(linghunciyu);
    connect(linghunciyu,SIGNAL(buttonSelected(int)),this,SLOT(LingHunCiYu()));
}

void LingHun::normal()
{
    Role::normal();
    Player *myself=dataInterface->getMyself();
    if (handArea->checkType("magic"))
        buttonArea->enable(3);
    if(myself->getToken(0)>=2)
        buttonArea->enable(4);
    if (handArea->checkSpecility(QStringLiteral("灵魂震爆")) && myself->getToken(0)>=3)
        buttonArea->enable(5);
    if (handArea->checkSpecility(QStringLiteral("灵魂赐予")) && myself->getToken(1)>=3)
        buttonArea->enable(6);
    unactionalCheck();
}

void LingHun::LingHunZhaoHuan()
{
    state=2201;
    playerArea->reset();
    handArea->reset();
    tipArea->reset();

    handArea->enableMagic();
    handArea->setQuota(1,7);

    decisionArea->enable(1);
    decisionArea->disable(0);
}

void LingHun::LingHunJingXiang()
{
    state=2202;
    playerArea->reset();
    handArea->reset();
    tipArea->reset();

    decisionArea->enable(1);
    decisionArea->disable(0);

    if(handArea->getHandCardItems().size()>3)
    {
        handArea->enableAll();
        handArea->setQuota(3);
    }
    else
        playerArea->enableAll();
    playerArea->setQuota(1);
}

void LingHun::LingHunZhenBao()
{
    state=2203;
    playerArea->reset();
    handArea->reset();
    tipArea->reset();

    playerArea->setQuota(1);

    handArea->enableSpecility(QStringLiteral("灵魂震爆"));
    handArea->setQuota(1);

    decisionArea->enable(1);
    decisionArea->disable(0);
}

void LingHun::LingHunCiYu()
{
    state=2204;
    playerArea->reset();
    handArea->reset();
    tipArea->reset();

    playerArea->setQuota(1);

    handArea->enableSpecility(QStringLiteral("灵魂赐予"));
    handArea->setQuota(1);

    decisionArea->enable(1);
    decisionArea->disable(0);
}

void LingHun::LingHunZhuanHuan()
{
    state=2205;
    Player *myself=dataInterface->getMyself();
    tipArea->setMsg(QStringLiteral("请选择要转换的灵魂："));
    decisionArea->enable(0);
    decisionArea->enable(1);

    if(myself->getToken(0)>0)
        tipArea->addBoxItem(QStringLiteral("1.将黄魂转化为蓝魂"));
    if(myself->getToken(1)>0)
        tipArea->addBoxItem(QStringLiteral("2.将蓝魂转化为黄魂"));
    tipArea->showBox();
}

void LingHun::LingHunLianJie(int harmPoint)
{
    state=2206;
    gui->reset();
    Player *myself=dataInterface->getMyself();
    decisionArea->enable(1);
    if(!lianJieUsed){
    tipArea->setMsg(QStringLiteral("是否发动灵魂连接？如是请选择目标"));
    playerArea->setQuota(1);
    decisionArea->disable(0);
    QList<Card*> handcards=dataInterface->getHandCards();
    Player *myself=dataInterface->getMyself();
    bool flag=true;
    int i;
    int n=handcards.size();
    decisionArea->enable(1);
    if(n<4)
    {
        flag=false;
        for(i=0;i<n;i++)
            if(handcards[i]->getElement()!="light")
            {
                flag=true;
                break;
            }
        if(myself->getToken(0)>1)
            flag=true;
    }
    if(flag)
        playerArea->enableMate();
    }
    else
    {
        decisionArea->enable(0);
        tipArea->setMsg(QStringLiteral("请选择要转移的伤害："));
        int min=myself->getToken(1)<harmPoint?myself->getToken(1):harmPoint;
        for(;min>=0;min--)
            tipArea->addBoxItem(QString::number(min));
        tipArea->showBox();
    }
}

void LingHun::LingHunZengFu()
{
    state=2207;
    gui->reset();
    tipArea->setMsg(QStringLiteral("是否发动灵魂增幅？"));
    decisionArea->enable(1);
    decisionArea->enable(0);
}

void LingHun::cardAnalyse()
{
    Role::cardAnalyse();
    switch(state)
    {
    case 2201:
        decisionArea->enable(0);
        break;
    case 2203:
    case 2204:
    case 2202:
        playerArea->enableAll();
        break;
    }
}

void LingHun::onOkClicked()
{
    Role::onOkClicked();
    QList<Card*>selectedCards;
    QList<Player*>selectedPlayers;

    QString command;
    QString cardID;
    QString sourceID;
    QString targetID;
    QString text;

    selectedCards=handArea->getSelectedCards();
    selectedPlayers=playerArea->getSelectedPlayers();

    network::Action* action;
    network::Respond* respond;

    switch(state)
    {
    case 2201:
        action = newAction(2201);
        foreach(Card*ptr,selectedCards){
            action->add_args(ptr->getID());
            dataInterface->removeHandCard(ptr);
        }

        emit sendCommand(network::MSG_ACTION, action);
        gui->reset();
        break;
    case 2202:
        action = newAction(2202);
        action->add_dst_ids(selectedPlayers[0]->getID());

        if(handArea->getHandCardItems().size()<=3)
        {
            action->add_args(100000);
            foreach(Card*ptr,dataInterface->getHandCards())
                dataInterface->removeHandCard(ptr);
        }
        else
        {
            foreach(Card*ptr,selectedCards){
                action->add_args(ptr->getID());
                dataInterface->removeHandCard(ptr);
            }
            command+=";";
        }

        emit sendCommand(network::MSG_ACTION, action);
        gui->reset();
        break;
    case 2203:
        action = newAction(2203);
        action->add_dst_ids(selectedPlayers[0]->getID());
        action->add_args(selectedCards[0]->getID());

        emit sendCommand(network::MSG_ACTION, action);
        gui->reset();
        break;
    case 2204:
        action = newAction(2204);
        action->add_dst_ids(selectedPlayers[0]->getID());
        action->add_args(selectedCards[0]->getID());

        emit sendCommand(network::MSG_ACTION, action);
        gui->reset();
        break;
    case 2205:
        respond = newRespond(2205);

        text=tipArea->getBoxCurrentText();
        if(text[0].digitValue()==1)
            respond->add_args(0);
        else
            respond->add_args(1);
        emit sendCommand(network::MSG_RESPOND, respond);
        gui->reset();
        break;
    case 2206:
        respond = newRespond(2206);

        if(!lianJieUsed)
        {
            respond->add_dst_ids(selectedPlayers[0]->getID());
            respond->add_args(1);
            start=true;
            lianJieUsed=true;
        }
        else
        {
            respond->add_args(tipArea->getBoxCurrentText().toInt());
        }
        emit sendCommand(network::MSG_RESPOND, respond);
        gui->reset();
        break;
    case 2207:
        respond = newRespond(2207);
        respond->add_args(1);
        start=true;
        emit sendCommand(network::MSG_RESPOND, respond);
        gui->reset();
        break;
    }
}

void LingHun::onCancelClicked()
{
    Role::onCancelClicked();
    QString command;

    network::Respond* respond;

    switch(state)
    {
    case 2201:
    case 2202:
    case 2203:
    case 2204:
        normal();
        break;
    case 2205:
        respond = newRespond(2205);
        emit sendCommand(network::MSG_RESPOND, respond);
        gui->reset();
        break;
    case 2206:
        respond = newRespond(2206);
        emit sendCommand(network::MSG_RESPOND, respond);
        gui->reset();
        break;
    case 2207:
        respond = newRespond(2207);
        emit sendCommand(network::MSG_RESPOND, respond);
        gui->reset();
        break;
    }
}

void LingHun::askForSkill(QString skill)
{
    Role::askForSkill(skill);
    if(skill==QStringLiteral("灵魂转换"))
        LingHunZhuanHuan();
    else if(skill==QStringLiteral("灵魂链接"))
        LingHunLianJie(command.split(';').at(3).toInt());
    else if(skill==QStringLiteral("灵魂增幅"))
        LingHunZengFu();
}


