set_xmakever("2.8.2")
set_project("WerewolfHotkey")
set_version("1.0.0")
set_license("MIT")
set_languages("c++23")
set_warnings("allextra")

add_rules("mode.release")

add_repositories("libxse-xrepo https://github.com/libxse/libxse-xrepo")
add_requires("commonlibsse-ng")

target("WerewolfHotkey")
    add_rules("@commonlibsse-ng/plugin", {
        name = "WerewolfHotkey",
        author = "you",
        description = "Press a hotkey (default K) to transform into a werewolf",
        email = "you@example.com"
    })
    add_files("src/*.cpp")
    add_packages("commonlibsse-ng")
