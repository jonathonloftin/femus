CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

get_filename_component(APP_FOLDER_NAME_PARENT ${CMAKE_CURRENT_LIST_DIR} NAME)

ADD_SUBDIRECTORY(ex1/)
ADD_SUBDIRECTORY(ex2/)
ADD_SUBDIRECTORY(ex3/)
ADD_SUBDIRECTORY(ex4/)
ADD_SUBDIRECTORY(ex5/)
ADD_SUBDIRECTORY(ex6/)
ADD_SUBDIRECTORY(ex7/)
ADD_SUBDIRECTORY(ex8/)
ADD_SUBDIRECTORY(ex9/)
ADD_SUBDIRECTORY(ex10/)
ADD_SUBDIRECTORY(ex11/)
ADD_SUBDIRECTORY(ex12/)


FILE(MAKE_DIRECTORY ${PROJECT_BINARY_DIR}/applications/MPM_FEM/input/)
FILE(COPY           ${PROJECT_SOURCE_DIR}/applications/MPM_FEM/input/ DESTINATION ${PROJECT_BINARY_DIR}/applications/MPM_FEM/input/)

