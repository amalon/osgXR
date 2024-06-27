include(CheckSourceCompiles)

set(CMAKE_REQUIRED_INCLUDES ${OPENSCENEGRAPH_INCLUDE_DIRS})

check_source_compiles(CXX
    "#include <osg/Camera>
    int main(int, char **)
    {
        (void)osg::Camera::FACE_CONTROLLED_BY_MULTIVIEW_SHADER;
        return 0;
    }"
    HAVE_OSG_MULTIVIEW)
