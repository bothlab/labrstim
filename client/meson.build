# Build configuration for the Labrstim client tool


add_languages('cpp')
qt5 = import('qt5')
qt5_dep = dependency('qt5', modules : ['Gui', 'SerialPort', 'Svg'])


client_src = [
    'main.cpp',
    'mainwindow.cpp',
    'settingsdialog.cpp',
    'labrstimclient.cpp'
]

client_headers = [
    'mainwindow.h',
    'settingsdialog.h',
    'labrstimclient.h'
]

client_ui = [
    'mainwindow.ui',
    'settingsdialog.ui'
]

client_qrc = [
    'res.qrc'
]

moc_files = qt5.preprocess(moc_headers: client_headers,
                           ui_files: client_ui,
                           qresources: client_qrc
)

executable('labrstimclient',
           client_src, moc_files,
           dependencies : qt5_dep
)
