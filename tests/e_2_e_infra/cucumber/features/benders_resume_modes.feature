Feature: Resume modes of Benders

@fast @short @resume
Scenario Outline: Benders runs for 2 iterations then we resume it from there to convergence
    Given the study path is "data_test/ieee96_base"
    When I set MAX_ITERATIONS to 2
    And I run benders with <procs> proc(s)
    Then the simulation succeeds
    And the problem_status is "limit reached"
    And the expected investment cost is 3.2615786040443573
    And the number of iterations is 2
    When I set MAX_ITERATIONS to -1
    And I set RESTART_MODE to "resume"
    And I run benders with <procs> proc(s)
    Then the simulation succeeds
    And the problem_status is "OPTIMAL"
    And the expected investment cost is 1.6307893020221786
    # The run continues: its iterations follow the first run's in the same outputs
    And the number of iterations is 5
    And the best iteration is 4
    And the last iteration file has its last iteration at 5 and its best at 4
    And "reportbenders.txt" contains "Building input" 2 times

    Examples:
      | procs |
      | 1     |
      | 2     |

@fast @short @resume 
Scenario Outline: Benders runs until optimality then we relaunch it with hot start 
    Given the study path is "data_test/ieee96_base"
    When I run benders with <procs> proc(s) 
    Then the simulation succeeds
    And the problem_status is "OPTIMAL"
    And the expected investment cost is 1.6307893020221786
    And the number of iterations is 4
    When I set RESTART_MODE to "hot_start"
    And I run benders with <procs> proc(s)
    Then the simulation succeeds
    And the number of iterations is 1

    Examples:
      | procs |
      | 1     |
      | 2     |

# A cold run takes 4 iterations: starting from the cuts of the first 2 saves one
@fast @short @resume
Scenario Outline: Benders runs for 2 iterations then we hot start it to convergence
    Given the study path is "data_test/ieee96_base"
    When I set MAX_ITERATIONS to 2
    And I run benders with <procs> proc(s)
    Then the simulation succeeds
    And the problem_status is "limit reached"
    And the number of iterations is 2
    When I set MAX_ITERATIONS to -1
    And I set RESTART_MODE to "hot_start"
    And I run benders with <procs> proc(s)
    Then the simulation succeeds
    And the problem_status is "OPTIMAL"
    And the expected investment cost is 1.6307893020221786
    # A new run: its outputs restart from iteration 1
    And the number of iterations is 3
    And the best iteration is 2

    Examples:
      | procs |
      | 1     |
      | 2     |
