build_example(
  NAME evaluation1
  SOURCE_FILES 
    evaluation1.cc
  HEADER_FILES
    rapidcsv.h
  LIBRARIES_TO_LINK
    ${libpoint-to-point}
    ${libtraffic-control}
    ${libinternet}
    ${libapplications}
    ${libflow-monitor}
    ${libstats}
)