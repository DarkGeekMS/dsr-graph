cmake_minimum_required( VERSION 3.0 )
PROJECT( components )


add_subdirectory( idserver )
#add_subdirectory( idserver_nogui )
add_subdirectory( viriatoDSR )
add_subdirectory( people_to_dsr )
add_subdirectory( change_graph )
#add_subdirectory( social_navigation )
#add_subdirectory( social_rules )
#add_subdirectory( watcherDSR )
add_subdirectory( crdt_rtps_dsr_tests )

add_subdirectory( elastic_band )
add_subdirectory( path_follower )
add_subdirectory( path_planner_astar )


