CREATE DATABASE IF NOT EXISTS SmartIrrigationDB;
USE SmartIrrigationDB;

CREATE TABLE IF NOT EXISTS Users (
    UserId    INT         PRIMARY KEY AUTO_INCREMENT,
    Username  VARCHAR(45) NOT NULL,
    Passwords VARCHAR(45) NOT NULL,
    UserRole  INT         NOT NULL
);
CREATE TABLE IF NOT EXISTS Sessions (
    SessionID  INT      PRIMARY KEY AUTO_INCREMENT,
    StartedAt  DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS SoilMoisture (
    SoilID     INT          PRIMARY KEY AUTO_INCREMENT,
    SessionID  INT          NOT NULL,
    RecordedAt DATETIME     DEFAULT CURRENT_TIMESTAMP,
    Moisture   DECIMAL(5,2) NOT NULL COMMENT 'percent 0-100',
    CONSTRAINT fk_soil_session
        FOREIGN KEY (SessionID) REFERENCES Sessions(SessionID)
        ON DELETE CASCADE ON UPDATE CASCADE
);

CREATE TABLE IF NOT EXISTS Temperature (
    TempID     INT          PRIMARY KEY AUTO_INCREMENT,
    SessionID  INT          NOT NULL,
    RecordedAt DATETIME     DEFAULT CURRENT_TIMESTAMP,
    TempValue  DECIMAL(5,2) NOT NULL COMMENT 'degrees Celsius',
    CONSTRAINT fk_temp_session
        FOREIGN KEY (SessionID) REFERENCES Sessions(SessionID)
        ON DELETE CASCADE ON UPDATE CASCADE
);

CREATE TABLE IF NOT EXISTS Humidity (
    HumID      INT          PRIMARY KEY AUTO_INCREMENT,
    SessionID  INT          NOT NULL,
    RecordedAt DATETIME     DEFAULT CURRENT_TIMESTAMP,
    HumValue   DECIMAL(5,2) NOT NULL COMMENT 'percent 0-100',
    CONSTRAINT fk_hum_session
        FOREIGN KEY (SessionID) REFERENCES Sessions(SessionID)
        ON DELETE CASCADE ON UPDATE CASCADE
);

CREATE TABLE IF NOT EXISTS WaterLevel (
    WaterID    INT          PRIMARY KEY AUTO_INCREMENT,
    SessionID  INT          NOT NULL,
    RecordedAt DATETIME     DEFAULT CURRENT_TIMESTAMP,
    LevelPct   DECIMAL(5,2) NOT NULL COMMENT 'percent 0-100',
    CONSTRAINT fk_water_session
        FOREIGN KEY (SessionID) REFERENCES Sessions(SessionID)
        ON DELETE CASCADE ON UPDATE CASCADE
);

CREATE TABLE IF NOT EXISTS Rainfall (
    RainID     INT         PRIMARY KEY AUTO_INCREMENT,
    SessionID  INT         NOT NULL,
    RecordedAt DATETIME    DEFAULT CURRENT_TIMESTAMP,
    RawValue   INT         NOT NULL  COMMENT 'ADC 0-4095',
    RainStatus VARCHAR(20) NOT NULL  COMMENT 'Raining / Not Raining',
    CONSTRAINT fk_rain_session
        FOREIGN KEY (SessionID) REFERENCES Sessions(SessionID)
        ON DELETE CASCADE ON UPDATE CASCADE
);

CREATE TABLE IF NOT EXISTS Motion (
    MotionID     INT         PRIMARY KEY AUTO_INCREMENT,
    SessionID    INT         NOT NULL,
    RecordedAt   DATETIME    DEFAULT CURRENT_TIMESTAMP,
    MotionStatus VARCHAR(20) NOT NULL  COMMENT 'Detected / None',
    CONSTRAINT fk_motion_session
        FOREIGN KEY (SessionID) REFERENCES Sessions(SessionID)
        ON DELETE CASCADE ON UPDATE CASCADE
);

INSERT INTO Users (Username, Passwords, UserRole) VALUES
('ADMIN',          'Admin123',  1),
('soil_farmer',    'Farmer123', 2),
('temphum_farmer', 'Farmer123', 3),
('water_farmer',   'Farmer123', 4),
('motion_farmer',  'Farmer123', 5),
('rain_farmer',    'Farmer123', 6);

