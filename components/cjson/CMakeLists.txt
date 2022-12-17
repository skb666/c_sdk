if(CONFIG_CJSON_ENABLED)
    ################# Add include #################
    list(APPEND ADD_INCLUDE "cJSON")
    # list(APPEND ADD_PRIVATE_INCLUDE "include_private")
    ###############################################

    ############## Add source files ###############
    list(APPEND ADD_SRCS "cJSON/cJSON.c")
    if(CONFIG_CJSON_UTILS_SUPPORT)
        list(APPEND ADD_SRCS "cJSON/cJSON_Utils.c")
    endif()
    # aux_source_directory(src ADD_SRCS)  # collect all source file in src dir, will set var ADD_SRCS
    # append_srcs_dir(ADD_SRCS "ejet/src")     # append source file in src dir to var ADD_SRCS
    # list(REMOVE_ITEM COMPONENT_SRCS "src/test.c")
    ###############################################

    ###### Add required/dependent components ######
    # list(APPEND ADD_REQUIREMENTS component1)
    ###############################################

    ###### Add link search path for requirements/libs ######
    # list(APPEND ADD_LINK_SEARCH_PATH "${CONFIG_TOOLCHAIN_PATH}/lib")
    # list(APPEND ADD_REQUIREMENTS
    #     nsl m pthread dl
    #     adif epump
    #     # ssl crypto
    # )  # add system libs, pthread or m(math) lib for example
    ###############################################

    ############ Add static libs ##################
    # if(CONFIG_COMPONENT1_INCLUDE_STATIC_LIB)
    #     list(APPEND ADD_STATIC_LIB "lib/libadif.a")
    # endif()
    ###############################################

    ############ Add dynamic libs ##################
    # list(APPEND ADD_DYNAMIC_LIB "lib/arch/v831/libmaix_nn.so"
    #                             "lib/arch/v831/libmaix_cam.so"
    # )
    ###############################################

    #### Add compile option for this component ####
    #### Just for this component, won't affect other 
    #### modules, including component that depend 
    #### on this component
    # list(APPEND ADD_DEFINITIONS_PRIVATE
    #     -DHAVE_EPOLL
    #     -DHAVE_EVENTFD
    # )
    ###############################################

    #### Add compile option for this component
    #### and components denpend on this component
    # list(APPEND ADD_DEFINITIONS
    #     -DUNIX
    #     -D_LINUX_
    #     # -DHAVE_OPENSSL
    # )
    ###############################################

    ############ Add static libs ##################
    #### Update parent's variables like CMAKE_C_LINK_FLAGS
    # set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -Wl,--start-group libmaix/libtest.a -ltest2 -Wl,--end-group" PARENT_SCOPE)
    ###############################################

    # register component, DYNAMIC or SHARED flags will make component compiled to dynamic(shared) lib
    register_component()
endif()
