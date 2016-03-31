/****************************************************************************
** Meta object code from reading C++ file 'CBar.h'
**
** Created by: The Qt Meta Object Compiler version 63 (Qt 4.8.6)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../CBar.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'CBar.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 63
#error "This file was generated using the moc from 4.8.6. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_CBar[] = {

 // content:
       6,       // revision
       0,       // classname
       0,    0, // classinfo
       8,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: signature, parameters, type, tag, flags
       6,    5,    5,    5, 0x05,

 // slots: signature, parameters, type, tag, flags
      31,   22,    5,    5, 0x0a,
      63,   52,    5,    5, 0x0a,
      89,   83,    5,    5, 0x0a,
     126,   52,    5,    5, 0x0a,
     146,   83,    5,    5, 0x0a,
     183,    5,    5,    5, 0x0a,
     208,  199,    5,    5, 0x0a,

       0        // eod
};

static const char qt_meta_stringdata_CBar[] = {
    "CBar\0\0update_signal()\0new_text\0"
    "setText(std::string)\0new_colour\0"
    "setFGColour(QColor)\0r,g,b\0"
    "setFGColour(uint8_t,uint8_t,uint8_t)\0"
    "setBGColour(QColor)\0"
    "setBGColour(uint8_t,uint8_t,uint8_t)\0"
    "switchColours()\0fraction\0"
    "setFilledFraction(float)\0"
};

void CBar::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        Q_ASSERT(staticMetaObject.cast(_o));
        CBar *_t = static_cast<CBar *>(_o);
        switch (_id) {
        case 0: _t->update_signal(); break;
        case 1: _t->setText((*reinterpret_cast< const std::string(*)>(_a[1]))); break;
        case 2: _t->setFGColour((*reinterpret_cast< const QColor(*)>(_a[1]))); break;
        case 3: _t->setFGColour((*reinterpret_cast< const uint8_t(*)>(_a[1])),(*reinterpret_cast< const uint8_t(*)>(_a[2])),(*reinterpret_cast< const uint8_t(*)>(_a[3]))); break;
        case 4: _t->setBGColour((*reinterpret_cast< const QColor(*)>(_a[1]))); break;
        case 5: _t->setBGColour((*reinterpret_cast< const uint8_t(*)>(_a[1])),(*reinterpret_cast< const uint8_t(*)>(_a[2])),(*reinterpret_cast< const uint8_t(*)>(_a[3]))); break;
        case 6: _t->switchColours(); break;
        case 7: _t->setFilledFraction((*reinterpret_cast< const float(*)>(_a[1]))); break;
        default: ;
        }
    }
}

const QMetaObjectExtraData CBar::staticMetaObjectExtraData = {
    0,  qt_static_metacall 
};

const QMetaObject CBar::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_CBar,
      qt_meta_data_CBar, &staticMetaObjectExtraData }
};

#ifdef Q_NO_DATA_RELOCATION
const QMetaObject &CBar::getStaticMetaObject() { return staticMetaObject; }
#endif //Q_NO_DATA_RELOCATION

const QMetaObject *CBar::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->metaObject : &staticMetaObject;
}

void *CBar::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_CBar))
        return static_cast<void*>(const_cast< CBar*>(this));
    return QWidget::qt_metacast(_clname);
}

int CBar::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 8)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    }
    return _id;
}

// SIGNAL 0
void CBar::update_signal()
{
    QMetaObject::activate(this, &staticMetaObject, 0, 0);
}
QT_END_MOC_NAMESPACE
