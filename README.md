# Posix_Shell
This project implements a user-defined interactive Posix based shell program using cpp

The requiremnets fulfilled in the project are:

    display prompt
    
    cd
    
    echo
    
    ls with all flags
    
    pwd
    
    auto complete commands using tab
    
    history
    
    simple signals as           
    
                                1. CTRL-Z It pushes any currently running foreground job into the background,
                                and change its state from running to stopped. 
                                
                                2. CTRL-C It interrupts any currently running foreground job, by sending it the
                                SIGINT signal. 
                                
                                3. CTRL-D It logs us out of our shell, without having any effect on the
                                actual terminal.
