[system programming lecture]
[20211569 이승연]

-project 1 baseline

csapp.{c,h}
        CS:APP3e functions

myshell.c
        <phase1>
        - cd : change the current working directory
        - history : display a list of previously executed commands.
                - !! : execute the previous command.
                - !n : execute the n-th command
        - exit : shut down the myshell program
        
        <phase2>
        - usage : command1 {args} | command2 {args}

        <phase3>
        - jobs : print the list of running or suspended background jobs
        - bg : change the suspended background job to a running background job
        - fg : change the background job to running background job
        - kill : terminate the job

