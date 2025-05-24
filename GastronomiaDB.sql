CREATE TABLE Recetas (
    Pregunta NVARCHAR(255) PRIMARY KEY,
    Respuesta NVARCHAR(MAX)
);

INSERT INTO Recetas (Pregunta, Respuesta) VALUES ('¿Cómo hacer una tortilla?', 'Bate huevos y cocina en sartén con aceite.');
