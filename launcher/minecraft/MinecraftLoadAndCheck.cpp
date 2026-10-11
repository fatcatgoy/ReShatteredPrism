#include "MinecraftLoadAndCheck.h"
#include "Application.h"
#include "MinecraftInstance.h"
#include "PackProfile.h"

MinecraftLoadAndCheck::MinecraftLoadAndCheck(MinecraftInstance* inst, Net::Mode netmode) : m_inst(inst), m_netmode(netmode) {}

void MinecraftLoadAndCheck::executeTask()
{
    if (m_netmode == Net::Mode::Online && m_inst->settings()->get("UseLatestMinecraftVersion").toBool()) {
        auto* components = m_inst->getPackProfile();
        auto releaseType = m_inst->settings()->get("UseLatestMinecraftVersionType").toString();
        if (components->updateLatestMinecraft(releaseType == "release") && APPLICATION->settings()->get("AutomaticJavaSwitch").toBool() &&
            m_inst->settings()->get("AutomaticJava").toBool() && m_inst->settings()->get("OverrideJavaLocation").toBool()) {
            m_inst->settings()->set("OverrideJavaLocation", false);
            m_inst->settings()->set("JavaPath", "");
        }
    }

    reloadComponents(m_netmode);
}

void MinecraftLoadAndCheck::reloadComponents(Net::Mode netmode)
{
    if (auto result = m_inst->getPackProfile()->reload(netmode, &m_task); !result) {
        emitFailed(result.error());
        return;
    }

    if (!m_task) {
        emitSucceeded();
        return;
    }
    connect(m_task.get(), &Task::finished, this, &MinecraftLoadAndCheck::onTaskFinished);
    propagateFromOther(m_task.get());
    if (m_task->isFinished()) {
        onTaskFinished();
    }
}

void MinecraftLoadAndCheck::onTaskFinished()
{
    if (isFinished() || !m_task) {
        return;
    }

    switch (m_task->getState()) {
        case Task::State::Succeeded:
            emitSucceeded();
            break;
        case Task::State::AbortedByUser:
            emitAborted();
            break;
        case Task::State::Failed:
            if (m_netmode == Net::Mode::Offline) {
                m_task.reset();
                m_netmode = Net::Mode::Online;
                setDetails(tr("Offline metadata is unavailable. Retrying metadata resolution online."));
                reloadComponents(m_netmode);
                break;
            }
            emitFailed(m_task->failReason());
            break;
        case Task::State::Inactive:
        case Task::State::Running:
            break;
    }
}

bool MinecraftLoadAndCheck::canAbort() const
{
    if (m_task) {
        return m_task->canAbort();
    }
    return true;
}

bool MinecraftLoadAndCheck::abort()
{
    if (m_task && m_task->canAbort()) {
        return m_task->abort();
    }
    return Task::abort();
}
