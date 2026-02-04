import { useEffect, useState } from 'react';
import { getWaterReadings, getLatestReading, supabase } from '../lib/supabase';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer, AreaChart, Area } from 'recharts';
import { Droplets, Gauge, TrendingUp, AlertCircle, Clock } from 'lucide-react';
import '../styles/dashboard.css';

export default function Dashboard() {
  const [latestReading, setLatestReading] = useState(null);
  const [readings, setReadings] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);

  useEffect(() => {
    const loadData = async () => {
      try {
        setLoading(true);
        const [latest, historical] = await Promise.all([
          getLatestReading(),
          getWaterReadings(100, 30),
        ]);

        if (latest) {
          setLatestReading(latest);
        }

        const formattedData = historical
          .reverse()
          .map((reading) => ({
            ...reading,
            time: new Date(reading.created_at).toLocaleTimeString('en-US', {
              hour: '2-digit',
              minute: '2-digit',
            }),
            date: new Date(reading.created_at).toLocaleDateString(),
          }));

        setReadings(formattedData);
        setError(null);
      } catch (err) {
        setError(err.message || 'Failed to load data');
        console.error('Error loading data:', err);
      } finally {
        setLoading(false);
      }
    };

    loadData();

    const subscription = supabase
      .channel('water_readings')
      .on(
        'postgres_changes',
        { event: 'INSERT', schema: 'public', table: 'water_readings' },
        (payload) => {
          const newReading = {
            ...payload.new,
            time: new Date(payload.new.created_at).toLocaleTimeString('en-US', {
              hour: '2-digit',
              minute: '2-digit',
            }),
            date: new Date(payload.new.created_at).toLocaleDateString(),
          };
          setLatestReading(newReading);
          setReadings((prev) => [...prev, newReading]);
        }
      )
      .subscribe();

    return () => {
      subscription.unsubscribe();
    };
  }, []);

  if (loading && !latestReading) {
    return (
      <div className="dashboard">
        <div className="loading">
          <div className="spinner"></div>
          <p>Loading water level data...</p>
        </div>
      </div>
    );
  }

  const getStatusColor = (level) => {
    if (level >= 80) return '#ef4444';
    if (level >= 50) return '#eab308';
    return '#ef4444';
  };

  const getAlarmStatus = (level) => {
    if (level >= 90) return 'High Level Alert';
    if (level <= 20) return 'Low Level Alert';
    return 'Normal';
  };

  const avgLevel = readings.length > 0
    ? (readings.reduce((sum, r) => sum + r.water_level, 0) / readings.length).toFixed(1)
    : 0;

  const maxLevel = readings.length > 0
    ? Math.max(...readings.map((r) => r.water_level))
    : 0;

  return (
    <div className="dashboard">
      <header className="header">
        <div className="header-content">
          <div className="logo-section">
            <Droplets className="logo-icon" />
            <h1>Water Level Monitor</h1>
          </div>
          <div className="status-badge" style={{ backgroundColor: getStatusColor(latestReading?.water_level || 0) }}>
            {getAlarmStatus(latestReading?.water_level || 0)}
          </div>
        </div>
      </header>

      {error && (
        <div className="error-banner">
          <AlertCircle size={20} />
          <p>{error}</p>
        </div>
      )}

      <main className="main-content">
        <div className="metrics-grid">
          <div className="metric-card current-level">
            <div className="metric-header">
              <Droplets size={24} />
              <span className="metric-label">Current Level</span>
            </div>
            <div className="metric-value">
              {latestReading ? `${latestReading.water_level.toFixed(1)}%` : '--'}
            </div>
            <div className="metric-detail">
              Distance: {latestReading ? `${latestReading.distance.toFixed(1)} cm` : '--'}
            </div>
            {latestReading && (
              <div className="metric-timestamp">
                <Clock size={14} />
                {new Date(latestReading.created_at).toLocaleString()}
              </div>
            )}
          </div>

          <div className="metric-card">
            <div className="metric-header">
              <Gauge size={24} />
              <span className="metric-label">Average Level</span>
            </div>
            <div className="metric-value">{avgLevel}%</div>
            <div className="metric-detail">Last 30 days</div>
          </div>

          <div className="metric-card">
            <div className="metric-header">
              <TrendingUp size={24} />
              <span className="metric-label">Peak Level</span>
            </div>
            <div className="metric-value">{maxLevel.toFixed(1)}%</div>
            <div className="metric-detail">Highest recorded</div>
          </div>
        </div>

        <div className="chart-section">
          <h2>Water Level Trend</h2>
          <ResponsiveContainer width="100%" height={300}>
            <AreaChart data={readings}>
              <defs>
                <linearGradient id="colorLevel" x1="0" y1="0" x2="0" y2="1">
                  <stop offset="5%" stopColor="#3b82f6" stopOpacity={0.8} />
                  <stop offset="95%" stopColor="#3b82f6" stopOpacity={0} />
                </linearGradient>
              </defs>
              <CartesianGrid strokeDasharray="3 3" stroke="#e5e7eb" />
              <XAxis
                dataKey="time"
                stroke="#6b7280"
                style={{ fontSize: '12px' }}
              />
              <YAxis
                stroke="#6b7280"
                style={{ fontSize: '12px' }}
                domain={[0, 100]}
              />
              <Tooltip
                contentStyle={{
                  backgroundColor: '#1f2937',
                  border: 'none',
                  borderRadius: '8px',
                  color: '#fff',
                }}
                formatter={(value) => `${value.toFixed(1)}%`}
                labelFormatter={(label) => `Time: ${label}`}
              />
              <Area
                type="monotone"
                dataKey="water_level"
                stroke="#3b82f6"
                fillOpacity={1}
                fill="url(#colorLevel)"
              />
            </AreaChart>
          </ResponsiveContainer>
        </div>

        <div className="chart-section">
          <h2>Distance Reading</h2>
          <ResponsiveContainer width="100%" height={250}>
            <LineChart data={readings}>
              <CartesianGrid strokeDasharray="3 3" stroke="#e5e7eb" />
              <XAxis dataKey="time" stroke="#6b7280" style={{ fontSize: '12px' }} />
              <YAxis stroke="#6b7280" style={{ fontSize: '12px' }} />
              <Tooltip
                contentStyle={{
                  backgroundColor: '#1f2937',
                  border: 'none',
                  borderRadius: '8px',
                  color: '#fff',
                }}
                formatter={(value) => `${value.toFixed(1)} cm`}
              />
              <Line
                type="monotone"
                dataKey="distance"
                stroke="#10b981"
                dot={false}
                strokeWidth={2}
              />
            </LineChart>
          </ResponsiveContainer>
        </div>

        {readings.length > 0 && (
          <div className="recent-readings">
            <h2>Recent Readings</h2>
            <div className="readings-list">
              {readings.slice(-10).reverse().map((reading, idx) => (
                <div key={idx} className="reading-item">
                  <div className="reading-time">
                    {new Date(reading.created_at).toLocaleString()}
                  </div>
                  <div className="reading-data">
                    <span className="reading-level">
                      {reading.water_level.toFixed(1)}%
                    </span>
                    <span className="reading-distance">
                      {reading.distance.toFixed(1)} cm
                    </span>
                  </div>
                </div>
              ))}
            </div>
          </div>
        )}
      </main>
    </div>
  );
}
