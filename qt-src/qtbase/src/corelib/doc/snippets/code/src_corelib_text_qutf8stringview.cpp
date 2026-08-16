// Copyright (C) 2020 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Marc Mutz <marc.mutz@kdab.com>
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

//! [0]
    void myfun1(QUtf8StringView sv);        // preferred
    void myfun2(const QUtf8StringView &sv); // compiles and works, but slower
//! [0]
