#include "ui/shortcuts/ShortcutManager.h"

#include <QWidget>
#include <QShortcut>

ShortcutManager::ShortcutManager(QWidget* parent)
    : QObject(parent)
{
    defaults_[Action::OpenFile] = QKeySequence::Open;
    defaults_[Action::PlayPause] = QKeySequence(Qt::Key_Space);
    defaults_[Action::Stop] = QKeySequence(Qt::Key_S);
    defaults_[Action::StepForward] = QKeySequence(Qt::Key_Right);
    defaults_[Action::StepBack] = QKeySequence(Qt::Key_Left);
    defaults_[Action::NextFrame] = QKeySequence(Qt::Key_L);
    defaults_[Action::PrevFrame] = QKeySequence(Qt::Key_J);
    defaults_[Action::ZoomIn] = QKeySequence::ZoomIn;
    defaults_[Action::ZoomOut] = QKeySequence::ZoomOut;
    defaults_[Action::ZoomFit] = QKeySequence(Qt::CTRL | Qt::Key_0);
    defaults_[Action::Zoom100] = QKeySequence(Qt::CTRL | Qt::Key_1);
    defaults_[Action::ToggleInfo] = QKeySequence(Qt::CTRL | Qt::Key_I);
    defaults_[Action::ToggleFilmstrip] = QKeySequence(Qt::CTRL | Qt::Key_F);
    defaults_[Action::StartAnalyze] = QKeySequence(Qt::Key_Return);
    defaults_[Action::StartExport] = QKeySequence(Qt::CTRL | Qt::Key_E);
    defaults_[Action::TogglePreview] = QKeySequence(Qt::Key_P);
}

QKeySequence ShortcutManager::shortcut(Action action) const
{
    const auto it = defaults_.find(action);
    return it != defaults_.end() ? it->second : QKeySequence();
}

void ShortcutManager::setShortcut(Action action, const QKeySequence& seq)
{
    defaults_[action] = seq;
}

void ShortcutManager::install(QWidget* target)
{
    for (auto& [action, keySeq] : defaults_) {
        auto* sc = new QShortcut(keySeq, target);
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sc, &QShortcut::activated, this, [this, action]() {
            emit triggered(action);
        });
        shortcuts_[action] = sc;
    }
}
