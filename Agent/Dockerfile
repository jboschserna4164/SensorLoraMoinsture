FROM python:3.11-slim

WORKDIR /app

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY app.py .

RUN mkdir /app/data

EXPOSE 4471

CMD ["gunicorn", "app:app", "--bind", "0.0.0.0:4471", "--workers", "2"]