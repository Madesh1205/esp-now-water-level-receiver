/*
  # Create Water Level Readings Table
  
  1. New Tables
    - `water_readings`
      - `id` (uuid, primary key) - unique identifier
      - `water_level` (float) - percentage level 0-100
      - `distance` (float) - distance in cm
      - `timestamp` (timestamp) - when reading was taken
      - `created_at` (timestamp) - when record was created
  
  2. Security
    - Enable RLS on `water_readings` table
    - Add policy for public read access to historical data
    - Add policy for authenticated inserts from Edge Function
*/

CREATE TABLE IF NOT EXISTS water_readings (
  id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
  water_level float NOT NULL,
  distance float NOT NULL,
  timestamp timestamptz NOT NULL,
  created_at timestamptz DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_water_readings_created_at ON water_readings(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_water_readings_timestamp ON water_readings(timestamp DESC);

ALTER TABLE water_readings ENABLE ROW LEVEL SECURITY;

CREATE POLICY "Public read access"
  ON water_readings FOR SELECT
  TO public
  USING (true);

CREATE POLICY "Service role insert"
  ON water_readings FOR INSERT
  TO service_role
  WITH CHECK (true);
