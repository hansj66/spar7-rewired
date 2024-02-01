idf_component_register(SRCS "console.c"
                    INCLUDE_DIRS .
                    REQUIRES 
                        console
                        driver
                        cmd_system
                        cmd_spar7
                    )
