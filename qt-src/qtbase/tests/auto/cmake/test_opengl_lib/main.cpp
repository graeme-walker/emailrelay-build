// Copyright (C) 2013 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Stephen Kelly <stephen.kelly@kdab.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only


#include <qglobal.h>
#include <QOpenGLFunctions>

int main(int argc, char **argv)
{
    QOpenGLFunctions functions;
    functions.glGetError();
    return 0;
}
