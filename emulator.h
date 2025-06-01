#ifndef EMULATOR_H
#define EMULATOR_H

#include <optional>

#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLWidget>

class Emulator : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT

private:
    std::optional<QOpenGLShaderProgram> shader_program;
//    QOpenGLVertexArrayObject vertex_array_object;
    QOpenGLBuffer vertex_buffer_object = QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    QOpenGLTexture texture = QOpenGLTexture(QOpenGLTexture::Target2D);

protected:
    void initializeGL() override;
    void paintGL() override;

public:
    using QOpenGLWidget::QOpenGLWidget;

    ~Emulator();

signals:

};

#endif // EMULATOR_H
